#include "MetadataManager.h"
#include "config.h"
#include "assert.h" 
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

MetadataManager *GlobalMetadataManagerPtr;


int MetadataManager::save(int current_version, int delta_size, int base_pos){
    //printf("-----------------------Saving One File FP-index-----------------------\n");
    std::string fp_name(Config::getInstance().getFpDeltaDedupFolderPath());
    fp_name.append("/fp_");
    fp_name.append(std::to_string(current_version));
    if(current_version == base_pos)
        fp_name.append("_base");
    else
        fp_name.append("_delta");

    int fd = open(fp_name.c_str(), O_RDWR | O_CREAT, 0777);
    if(fd < 0){
        perror("Saving fp index error, the reason is ");
        exit(-1);
    }

    int count = 0;
    if(current_version == base_pos){
        for(auto item : this->fp_table_base){
            write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
            write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
            count++;
        }
    }else if(current_version <= (base_pos + delta_size)){
        for(auto item : this->fp_table_delta){
            int n = 0;
            write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
            write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
            count++;
        }
    }else{
        printf("Saving fp error\n");
        exit(-1);
    }

    if(current_version == (base_pos + delta_size)){
        fp_table_base.clear(); 
    }

    fp_table_delta.clear();

    //printf("total item %d\n", count);
    close(fd);
    return 0;
}

int MetadataManager::load(){
    printf("-----------------------Loading FP-index-----------------------\n");
    printf("Loading index..\n");

    unsigned char* metadata_cache = (unsigned char*)malloc(FILE_CACHE);
    int fd = open(this->metadata_file_path.c_str(), O_RDONLY);
    if(fd < 0)
        printf("MetadataManager::load error\n");
    int n = read(fd, metadata_cache, FILE_CACHE);
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

LookupResult MetadataManager::dedupLookup(SHA1FP sha1, bool in_delta){
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

int MetadataManager::addNewEntry(SHA1FP sha1, ENTRY_VALUE value, bool in_delta){
    if(in_delta)
        this->fp_table_delta.emplace(sha1, value);
    else    
        this->fp_table_base.emplace(sha1, value);
    return 0;
}

int MetadataManager::addRefCnt(const SHA1FP sha1){
    auto dedupIter = this->fp_table_added.find(sha1);
    if(dedupIter != this->fp_table_added.end())
        return ++dedupIter->second.ref_cnt;
    dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_added.end())
        return ++dedupIter->second.ref_cnt;
    printf("addRefCnt: did not find\n");
}

ENTRY_VALUE MetadataManager::getEntry(const SHA1FP sha1){
    return this->fp_table_origin[sha1];
}