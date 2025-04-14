#include "MetadataManager.h"
#include "config.h"
#include "assert.h"
#include "deltaDedup_stats.h" 
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include <regex>


MetadataManager *GlobalMetadataManagerPtr;

int MetadataManager::saveVersion(int current_version, FILE_ATTR file_attr){
    //printf("-----------------------Saving One File FP-index-----------------------\n");
    std::string fp_name = genFPname(current_version, file_attr);

    int fd = open(fp_name.c_str(), O_RDWR | O_CREAT, 0777);
    if(fd < 0){
        perror("Saving fp index error, the reason is ");
        exit(-1);
    }

    if(file_attr == ATTR_BASE){
        for(auto item : this->fp_table_base){
            write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
            write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        }
    }else if(file_attr == ATTR_DELTA || file_attr == ATTR_SLBASE){
        for(auto item : this->fp_table_delta){
            write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
            write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        }
    }else{
        printf("Saving fp error\n");
        exit(-1);
    }

    fp_table_delta.clear();
    close(fd);
    return 0;
}

string MetadataManager::genFPname(int version, FILE_ATTR file_attr){
    std::string fp_name(Config::getInstance().getFpDeltaDedupFolderPath());
    fp_name.append("/fp_");
    fp_name.append(std::to_string(version));
    if(file_attr == ATTR_DELTA)
        fp_name.append("_delta");
    else if(file_attr == ATTR_BASE)
        fp_name.append("_base");
    else if(file_attr == ATTR_SLBASE)
        fp_name.append("_slbase");
    return fp_name;
}


// DEDEUP_DELTA load fp&entry metadata
int MetadataManager::loadVersion(int version, bool is_restore){
    // 恢复时：如果该版本是base，只需加载base的fp；如果该版本是delta，需要加载它前面一个base的fp和它自己的fp
    // 写入时：因为加载前一个版本所以加载base的fp；如果该版本是delta，需要加载它前面一个base的fp   (如果写入base本身，不需要加载fp)
    vector<pair<string, double>> attr_vec = loadAllDedupRatios();
    auto [attr, dr] = attr_vec.at(version);
    FILE_ATTR file_attr = string_to_attr(attr);

    string fp_name = genFPname(version, file_attr);
    if(file_attr == ATTR_BASE){
        loadDeltaDedupFp(fp_name,is_restore);
    }else{
        // 写入时加载元数据，不需要加载delta版本的fp
        // int base_file_version = findNearestBaseBefore(attr_vec, version);
        // string base_file = genFPname(base_file_version, ATTR_BASE);
        // loadDeltaDedupFp(base_file,is_restore);
        auto [last_slbase_version, last_base_version]= findNearestBaseBefore(attr_vec, version);
        if(last_slbase_version != -1){
            string slbase_file = genFPname(last_slbase_version, ATTR_SLBASE);
            loadDeltaDedupFp(slbase_file,is_restore);
        }
        if(last_base_version != -1){
            string base_file = genFPname(last_base_version, ATTR_BASE);
            loadDeltaDedupFp(base_file,is_restore);
        }
        if(is_restore && file_attr == ATTR_DELTA){
            loadDeltaDedupFp(fp_name,is_restore);
        }
    }
}

void MetadataManager::loadDeltaDedupFp(std::string fp_name, bool is_restore){
    printf("-----------------------Loading FP-index DeltaDedup-----------------------\n");
    printf("Loading index: %s\n", fp_name.c_str());

    unsigned char* metadata_cache = (unsigned char*)malloc(FILE_CACHE);
    int fd = open(fp_name.c_str(), O_RDONLY);
    if(fd < 0)
        printf("MetadataManager::load error\n");
    
    // 这里读一次，已经默认了fp的总大小不回超过FILE_CACHE
    int n = read(fd, metadata_cache, FILE_CACHE);
    int meta_size = sizeof(SHA1FP) + sizeof(ENTRY_VALUE);
    int entry_count = n/meta_size;
    SHA1FP tmp_fp;
    ENTRY_VALUE tmp_value;
    int tmp_base_container_index = 0;

    for(int i=0; i<=entry_count-1; i++){
        memcpy(&tmp_fp, metadata_cache+i*meta_size, sizeof(SHA1FP));
        memcpy(&tmp_value, metadata_cache+i*meta_size + sizeof(SHA1FP), sizeof(ENTRY_VALUE));

        if(is_restore){
            // 找到base容器的最大值
            if(this->base_container_max_value==0 && tmp_value.container_number > tmp_base_container_index){
                tmp_base_container_index = tmp_value.container_number;
            }
            if(i==entry_count-1 && this->base_container_max_value==0){
                this->base_container_max_value = tmp_base_container_index;
            }
            
            this->fp_table_origin.emplace(tmp_fp, tmp_value);
        }else{
            this->fp_table_base.emplace(tmp_fp, tmp_value);
        }  
    }

    close(fd);
    free(metadata_cache);
    printf("metadata table load %d items\n", entry_count);
}

int MetadataManager::load(){
    printf("-----------------------Loading FP-index-----------------------\n");
    printf("Loading index..\n");

    unsigned char* metadata_cache = (unsigned char*)malloc(MAX_FILE_CACHE);
    int fd = open(this->metadata_file_path.c_str(), O_RDONLY);
    if(fd < 0)
        printf("MetadataManager::load error\n");
    int n = read(fd, metadata_cache, MAX_FILE_CACHE);
    int meta_size = sizeof(SHA1FP) + sizeof(ENTRY_VALUE);
    int entry_count = n/meta_size;
    SHA1FP tmp_fp;
    ENTRY_VALUE tmp_value;

    for(int i=0; i<=entry_count-1; i++){
        memcpy(&tmp_fp, metadata_cache+i*meta_size, sizeof(SHA1FP));
        memcpy(&tmp_value, metadata_cache+i*meta_size + sizeof(SHA1FP), sizeof(ENTRY_VALUE));

        this->fp_table_origin.emplace(tmp_fp, tmp_value);
    }

    close(fd);
    free(metadata_cache);
    printf("metadata table load %d items\n", entry_count);
}

int MetadataManager::save(){
    printf("-----------------------Saving FP-index-----------------------\n");
    int fd = open(this->metadata_file_path.c_str(), O_WRONLY | O_CREAT, 0777);
    if(fd < 0){
        perror("Saving fp index error, the reason is ");
        exit(-1);
    }
    lseek(fd, 0, SEEK_SET);
    ftruncate(fd,0);

    int count = 0;
    
    for(auto item : this->fp_table_added){
        write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
        write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        count++;
    }
    printf("New added item %d\n", count);

    for(auto item : this->fp_table_origin){
        write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
        write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        count++;
    }
    printf("total item %d\n", count);

    close(fd);
}


LookupResult MetadataManager::dedupLookup(SHA1FP sha1){
    auto dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_origin.end()){
        return Dedup;
    }

    dedupIter = this->fp_table_added.find(sha1);
    if(dedupIter != this->fp_table_added.end()){
        return Dedup;
    }

    return Unique;
}

LookupResult MetadataManager::dedupLookup(SHA1FP sha1, FILE_ATTR file_attr){
    auto dedupIter = this->fp_table_base.find(sha1);
    if(dedupIter != this->fp_table_base.end())
        return Dedup;


    dedupIter = this->fp_table_delta.find(sha1);
    if(dedupIter != this->fp_table_delta.end())
        return Dedup;
    
    return Unique;
}

int MetadataManager::addNewEntry(SHA1FP sha1, ENTRY_VALUE value){
    this->fp_table_added.emplace(sha1, value);
    return 0;
}

int MetadataManager::addNewEntry(SHA1FP sha1, ENTRY_VALUE value, FILE_ATTR file_attr){
    if(file_attr != ATTR_BASE)
        this->fp_table_delta.emplace(sha1, value);
    else    
        this->fp_table_base.emplace(sha1, value);
    return 0;
}

// delta版本的重复块可以来自delta和base，base只来自base
int MetadataManager::addRefCnt(const SHA1FP sha1, bool in_delta){
    if(in_delta){
        auto dedupIter = this->fp_table_delta.find(sha1);
        if(dedupIter != this->fp_table_delta.end()){
            return ++dedupIter->second.ref_cnt;
        } 
    }
    auto dedupIter = this->fp_table_base.find(sha1);
    if(dedupIter != this->fp_table_base.end()){
        return ++dedupIter->second.ref_cnt;
    }
    printf("addRefCnt: did not find\n");
}

int MetadataManager::addRefCnt(const SHA1FP sha1){
    auto dedupIter = this->fp_table_added.find(sha1);
    if(dedupIter != this->fp_table_added.end())
        return ++dedupIter->second.ref_cnt;
    dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_origin.end())
        return ++dedupIter->second.ref_cnt;
    printf("addRefCnt: did not find\n");
}

ENTRY_VALUE MetadataManager::getEntry(const SHA1FP sha1){
    return this->fp_table_origin[sha1];
}

int MetadataManager::getBaseContainerMaxValue(){
    return this->base_container_max_value;
}

void MetadataManager::clear_base(){
    fp_table_base.clear(); 
}

// int MetadataManager::save(int current_version, int delta_size, int base_pos){
//     //printf("-----------------------Saving One File FP-index-----------------------\n");
//     std::string fp_name(Config::getInstance().getFpDeltaDedupFolderPath());
//     fp_name.append("/fp_");
//     fp_name.append(std::to_string(current_version));
//     if(current_version == base_pos)
//         fp_name.append("_base");
//     else
//         fp_name.append("_delta");

//     int fd = open(fp_name.c_str(), O_RDWR | O_CREAT, 0777);
//     if(fd < 0){
//         perror("Saving fp index error, the reason is ");
//         exit(-1);
//     }

//     int count = 0;
//     if(current_version == base_pos){
//         for(auto item : this->fp_table_base){
//             write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
//             write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
//             count++;
//         }
//     }else if(current_version <= (base_pos + delta_size)){
//         for(auto item : this->fp_table_delta){
//             int n = 0;
//             write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
//             write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
//             count++;
//         }
//     }else{
//         printf("Saving fp error\n");
//         exit(-1);
//     }

//     if(current_version == (base_pos + delta_size)){
//         fp_table_base.clear(); 
//     }

//     fp_table_delta.clear();

//     //printf("total item %d\n", count);
//     close(fd);
//     return 0;
// }
