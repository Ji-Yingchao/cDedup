#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/sendfile.h>

#include "fastcdc.h"
#include "full_file_deduplicater.h"
#include "merkle_tree.h"
#include "MetadataManager.h"
#include "ContainerCache.h"
#include "ChunkCache.h"
#include "general.h"
#include "FAA.h"
#include "config.h"
#include "utils/cJSON.h"


uint32_t rev_container_cnt = 0;
unsigned char rev_container_buf[CONTAINER_SIZE]={0};
unsigned char tmp_buf[CONTAINER_SIZE]={0};


int (*chunking) (unsigned char*p, int n);

uint32_t getFilesNum(const char* dirPath){
    int ans = 0;
    DIR *dir = opendir(dirPath);
    if(!dir){
        printf("getFilesNum opendir error, id %d, %s, the dir is %s\n", 
        errno, strerror(errno), dirPath);
        closedir(dir);
        exit(-1);
    }
    struct dirent* ptr;
    while(readdir(dir)) ans++;
    closedir(dir);
    return ans-2;
}

void saveFileRecipe(std::vector<std::string> file_recipe, const char* fileRecipesPath){
    int n_version = getFilesNum(fileRecipesPath);
    std::string recipe_name(fileRecipesPath);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(n_version));
    int fd = open(recipe_name.data(), O_RDWR | O_CREAT, 0777);
    if(fd < 0){ 
        printf("saveFileRecipe open error, id %d, %s\n", errno, strerror(errno)); 
        exit(-1);
    }
    for(auto x : file_recipe){
        if(write(fd, x.data(), SHA_DIGEST_LENGTH) < 0)
            printf("save recipe write error\n");
    }
    close(fd);
}

std::string getRecipeNameFromVersion(uint8_t restore_version, const char* file_recipe_path){
    std::string recipe_name(file_recipe_path);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(restore_version));
    return recipe_name;
}

bool fileRecipeExist(uint8_t restore_version, const char* file_recipe_path){
    if((access(getRecipeNameFromVersion(restore_version, file_recipe_path).data(), F_OK)) != -1)    
        return true;    
 
    return false;
}

std::vector<std::string> getFileRecipe(uint8_t restore_version, const char* file_recipe_path){
    if(!fileRecipeExist(restore_version, file_recipe_path)){
        printf("Restore version %d not exist!!!\n", restore_version); 
        exit(-1);
    }

    std::string recipe_name = getRecipeNameFromVersion(restore_version, file_recipe_path);
    int fd = open(recipe_name.data(), O_RDONLY, 0777);
    if(fd < 0){
        printf("getFileRecipe open error, %s, %s\n", strerror(errno), recipe_name.data());
        exit(-1);
    }
    char fp_buf[SHA_DIGEST_LENGTH]={0};
    std::vector<std::string> ans;

    while(1){
        int n = read(fd, fp_buf, SHA_DIGEST_LENGTH);
        if(n == 0){
            break;
        }else if(n < 0){
            printf("getFileRecipe error, %s, %s\n", strerror(errno), recipe_name.data());
            exit(-1);
        }else{
            ans.push_back(std::string(fp_buf, SHA_DIGEST_LENGTH));
        }
    }
    close(fd);
    return ans;
}

void saveContainer(int container_index, unsigned char* container_buf, unsigned int len, const char* containersPath){
    std::string container_name(containersPath);
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDWR | O_CREAT, 0777);
    if(write(fd, container_buf, len) != len){
        printf("saveContainer write error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
    close(fd);
    container_name.append("r");
    fd = open(container_name.data(), O_WRONLY | O_CREAT, 0777);
    //printf("rev_container_cnt: %d\n",rev_container_cnt);
    write(fd, &rev_container_cnt, sizeof(uint32_t));
    write(fd, &rev_container_buf, rev_container_cnt * sizeof(SHA1FP));
    close(fd);
}

void saveChunkToContainer(unsigned int& container_buf_pointer, unsigned char* container_buf, 
                          uint32_t& container_index, uint32_t& container_inner_offset, uint16_t& container_inner_index,
                          int chunk_length, int file_offset, unsigned char* file_cache, void* SHA_buf,
                          const char* containers_path){
    // flush
    if(container_buf_pointer + chunk_length >= CONTAINER_SIZE){
        saveContainer(container_index, container_buf, container_buf_pointer, containers_path);
        memset(container_buf, 0, CONTAINER_SIZE);
        container_index++;
        container_inner_offset = 0;
        container_buf_pointer = 0;
        container_inner_index = 0;

        rev_container_cnt = 0;
    }

    // container buffer
    memcpy(container_buf + container_buf_pointer, file_cache + file_offset, chunk_length);
    memcpy(rev_container_buf + sizeof(SHA1FP)*rev_container_cnt, SHA_buf, sizeof(SHA1FP));

    // SHA1FP tt;
    // memcpy(&tt, SHA_buf, sizeof(SHA1FP));
    // if(GlobalMetadataManagerPtr->dedupLookup(tt) == Unique) {
    //     printf("asdfasdfasdfasdfasdfasdfa\n");
    //     exit(-1);
    // }

}

void del_seg(std::string s, int oft, int len){
    int fd = open(s.c_str(), O_RDONLY, 0777);
    std::string s2 = s + "tmp";
    int fd2 = open(s2.c_str(), O_RDWR | O_CREAT, 0777);

    read(fd, tmp_buf, oft);
    write(fd2, tmp_buf, oft);

    read(fd, tmp_buf, len);

    int tt = read(fd, tmp_buf, CONTAINER_SIZE);
    write(fd2, tmp_buf, tt);

    close(fd);
    close(fd2);

    remove(s.c_str());
    if(oft == 0 && tt == 0)
        remove(s2.c_str());
    else
        rename(s2.c_str(), s.c_str());
}

void container_del_chunk(int ctr_id, SHA1FP del_sha, int oft, int len){
    std::string ctr_path = std::string(Config::getInstance().getContainersPath().c_str());
    ctr_path.append("/container");
    ctr_path.append(std::to_string(ctr_id));
    del_seg(ctr_path, oft, len);
    
    ctr_path.append("r");
    int fd = open(ctr_path.c_str(), O_RDONLY, 0777);
    read(fd, &rev_container_cnt, sizeof(uint32_t));
    if(rev_container_cnt == 1){
        close(fd);
        remove(ctr_path.c_str());
        return;
    }
    int d_pos = -1;
    for(int a = 0; a < rev_container_cnt; a++){
        SHA1FP sha1t;
        read(fd, &sha1t, sizeof(SHA1FP));
        TupleEqualer te;
        if(te(sha1t, del_sha)){
            d_pos = a;
            continue;
        }
        GlobalMetadataManagerPtr->chunkOffsetDec(sha1t, oft, len);
    }
    close(fd);
    rev_container_cnt--;

    fd = open(ctr_path.c_str(), O_WRONLY | O_CREAT, 0777);
    lseek(fd, 0, SEEK_SET);
    write(fd, &rev_container_cnt, sizeof(uint32_t));
    close(fd);

    del_seg(ctr_path, sizeof(uint32_t) + d_pos*sizeof(SHA1FP), sizeof(SHA1FP));
}

void flushAssemblingBuffer(int fd, unsigned char* buf, int len){
    if(write(fd, buf, len) != len){
        printf("Restore, write file error!!!\n");
        exit(-1);
    }
    //close(fd);
}

int main(int argc, char** argv){
    // 超级权限
    setuid(0);

    // 参数解析
    Config::getInstance().parse_argument(argc, argv);
    
    // 一次任务的总时间
    // MFDedup并没有计算load FP-index的时间
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    
    if(Config::getInstance().getChunkingMethod() == FULL_FILE){
        FullFileDeduplicater ffd(Config::getInstance().getFullFileStoragePath(), 
                                 Config::getInstance().getFullFileFingerprintsPath());
        if(Config::getInstance().getTaskType() == TASK_WRITE){
            ffd.writeFile(Config::getInstance().getInputPath().c_str());

            if(ffd.get_file_exist())
                printf("Your input file is existed.\n");
            else
                printf("Your input file is not existed.\n");

            printf("The file id is %d\n", ffd.get_file_id());
        }
        else if(Config::getInstance().getTaskType() == TASK_RESTORE){
            ffd.restoreFile(Config::getInstance().getRestoreId(), 
                            Config::getInstance().getRestorePath().c_str());
        }
        else if(Config::getInstance().getTaskType() == TASK_DELETE){
            ffd.deleteFile(Config::getInstance().getDeleteId());
        }
        return 0;
    }else if(Config::getInstance().getChunkingMethod() == CDC){
        int NC_level = Config::getInstance().getNormalLevel();
        int avg_size = Config::getInstance().getAvgChunkSize();
        fastCDC_init(avg_size, NC_level);

        if(NC_level == 0)
            chunking = FastCDC_without_NC;
        else if(1<=NC_level && NC_level<=3)
            chunking = FastCDC_with_NC;
        else{
            printf("Invalid NC level: %d\n", NC_level);
        }
    }else if(Config::getInstance().getChunkingMethod() == FSC){
        int avg_size = Config::getInstance().getAvgChunkSize();
        if(avg_size == 4*1024){
            chunking = FSC_4;
        }else if(avg_size == 8*1024){
            chunking = FSC_8;
        }else if(avg_size == 16*1024){
            chunking = FSC_16;
        }else{
            printf("Invalid fixed size, the support size is 4 or 8 or 16\n");
            return 0;
        }
    }
    
    GlobalMetadataManagerPtr = new MetadataManager(Config::getInstance().getFingerprintsFilePath().c_str());
    int current_version = getFilesNum(Config::getInstance().getFileRecipesPath().c_str());
    if(current_version != 0){
        GlobalMetadataManagerPtr->load();
    }

    if(Config::getInstance().getTaskType() == TASK_WRITE){
        int idf = open(Config::getInstance().getInputPath().c_str(), O_RDONLY, 0777);
        if(idf < 0){
            printf("open file error, id %d, %s\n", errno, strerror(errno));
            exit(-1);
        }

        unsigned char* file_cache = (unsigned char*)malloc(FILE_CACHE);
        struct SHA1FP sha1_fp;
        unsigned char SHA_buf[SHA_DIGEST_LENGTH]={0};
        unsigned char meta_data_buf[META_DATA_SIZE]={0};
        std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹

        // metadata entry(except FP)
        uint32_t container_index = getFilesNum(Config::getInstance().getContainersPath().c_str());
        uint32_t container_inner_offset = 0;
        uint16_t chunk_length = 0;
        uint16_t container_inner_index = 0;

        unsigned char container_buf[CONTAINER_SIZE]={0};
        unsigned int container_buf_pointer = 0;
        uint32_t file_offset = 0;
        uint32_t n_read = 0;

        MerkleTree *mt = nullptr;
        struct ENTRY_VALUE entry_value;

        // 重删统计
        long long dedup_chunks = 0;
        long long sum_chunks = 0;
        long long sum_size = 0;
        long long dedup_size = 0;
        long long  hash_collision_sum = 0;

        // 梅克尔树重删 由于树内部块没参与重删，可能会降低重删率。
        // 暂不支持大于FILE CACHE大小文件的重删
        if(Config::getInstance().getMerkleTree()){
            std::vector<L0_node> L0_nodes;
            n_read = read(idf, file_cache, FILE_CACHE);
            while(file_offset < n_read){  
                chunk_length = chunking(file_cache + file_offset, n_read - file_offset);
                memset(SHA_buf, 0, SHA_DIGEST_LENGTH);
                SHA1(file_cache + file_offset, chunk_length, SHA_buf);
                L0_nodes.emplace_back(L0_node(file_offset, chunk_length, SHA_buf));

                file_offset += chunk_length;
                file_recipe.push_back(std::string((char*)SHA_buf, SHA_DIGEST_LENGTH));
            }

            char** merkleMeta = (char**)malloc(sizeof(char*)*7);
            merkleMeta[0] = const_cast<char*>(Config::getInstance().getFingerprintsFilePath().c_str());
            merkleMeta[1] = const_cast<char*>(Config::getInstance().getMTL1().c_str());
            merkleMeta[2] = const_cast<char*>(Config::getInstance().getMTL2().c_str());
            merkleMeta[3] = const_cast<char*>(Config::getInstance().getMTL3().c_str());
            merkleMeta[4] = const_cast<char*>(Config::getInstance().getMTL4().c_str());
            merkleMeta[5] = const_cast<char*>(Config::getInstance().getMTL5().c_str());
            merkleMeta[6] = const_cast<char*>(Config::getInstance().getMTL6().c_str());
            mt = new MerkleTree(merkleMeta, 10, 6, L0_nodes);
            mt->buildTree(L0_nodes);
            mt->markNonDuplicateNodes();
            
            // save non-duplicate chunks to container
            for(auto &x: L0_nodes){
                // Statistic
                sum_chunks ++;
                sum_size += x.chunk_length;
                if(!x.found){
                    saveChunkToContainer(container_buf_pointer, container_buf, 
                        container_index, container_inner_offset, container_inner_index,
                        x.chunk_length, x.file_offset, file_cache, (void*)x.SHA1_hash.c_str(),
                        Config::getInstance().getContainersPath().c_str());
                }else{
                    dedup_chunks ++;
                    dedup_size += x.chunk_length;
                }
            }
            goto writend;
        }

        // 普通分块重删，来一个块查寻一次，然后把non-duplicate chunk保存到container去
        for(;;){
            file_offset = 0;
            n_read = read(idf, file_cache, FILE_CACHE);
            if(n_read <= 0){
                break;
            }

            while(file_offset < n_read){  
                // Chunking
                chunk_length = chunking(file_cache + file_offset, n_read - file_offset);
                
                // Compute hash
                memset(&sha1_fp, 0, sizeof(struct SHA1FP));
                SHA1(file_cache + file_offset, chunk_length, (uint8_t*)&sha1_fp);

                // Insert fingerprint into file recipe
                file_recipe.push_back(std::string((char*)&sha1_fp, sizeof(struct SHA1FP)));

                // Statistic
                sum_chunks ++;
                sum_size += chunk_length;
                
                // lookup fingerprint
                LookupResult lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp);

                // 
                if(lookup_result == Unique){
                     // save chunk itself
                    saveChunkToContainer(container_buf_pointer, container_buf, 
                                        container_index, container_inner_offset, container_inner_index,
                                        chunk_length, file_offset, file_cache, (void*)&sha1_fp,
                                        Config::getInstance().getContainersPath().c_str());

                    // save chunk metadata
                    entry_value.container_number = container_index;
                    entry_value.offset = container_inner_offset;
                    entry_value.chunk_length = chunk_length;
                    entry_value.container_inner_index = container_inner_index;
                    entry_value.ref_cnt = 1;
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value);
                    
                    // rev
                    container_inner_offset += chunk_length;
                    container_buf_pointer += chunk_length;
                    container_inner_index ++;
                    rev_container_cnt ++;

                }else if(lookup_result == Dedup){
                    GlobalMetadataManagerPtr->addRefCnt(sha1_fp);
                    // 重删统计
                    dedup_chunks ++;
                    dedup_size += chunk_length;

                    // 可能遇到哈希碰撞
                    // TODO
                }
                file_offset += chunk_length;
            }
        }

    writend:       
        //  flush最后一个container
        if(container_buf_pointer > 0)
            saveContainer(container_index, container_buf, 
                          container_buf_pointer, Config::getInstance().getContainersPath().c_str());
        
        // flush file_recipe
        saveFileRecipe(file_recipe, Config::getInstance().getFileRecipesPath().c_str());

        // save metadata entry
        GlobalMetadataManagerPtr->save();

        // 写文件 - 重删统计
        printf("-----------------------Dedup statics----------------------\n");
        printf("Hash collision num %lld\n", hash_collision_sum); // should be zero
        printf("Sum chunks num %lld\n",     sum_chunks);
        printf("Sum data size %lld\n",      sum_size);
        printf("Dedup chunks num %lld\n",   dedup_chunks);
        printf("Dedup data size %lld\n",    dedup_size);
        printf("Dedup Ratio %.2f%\n",     double(dedup_size) / double(sum_size) *100);
        close(idf);

    }else if(Config::getInstance().getTaskType() == TASK_RESTORE){
        int restore_size = 0;

        if(!fileRecipeExist(Config::getInstance().getRestoreVersion(),
                            Config::getInstance().getFileRecipesPath().c_str())){
            printf("Version %d not exist!\n", Config::getInstance().getRestoreVersion());
            return 0;
        }
        
        unsigned char* assembling_buffer = (unsigned char*)malloc(FILE_CACHE);
        memset(assembling_buffer, 0, FILE_CACHE);
        int write_buffer_offset = 0;

        //recipe
        std::vector<std::string> file_recipe = getFileRecipe(Config::getInstance().getRestoreVersion(),
                                                             Config::getInstance().getFileRecipesPath().c_str());

        //组装
        if(Config::getInstance().getRestoreMethod() == CONTAINER_CACHE ||
           Config::getInstance().getRestoreMethod() == CHUNK_CACHE){
            int fd = open(Config::getInstance().getRestorePath().c_str(), O_RDWR | O_CREAT, 0777);
            if(fd < 0){
                printf("无法写文件!!! %s\n", strerror(errno));
                exit(-1);
            }

            Cache* cc;
            if(Config::getInstance().getRestoreMethod() == CONTAINER_CACHE){
                cc = new ContainerCache(Config::getInstance().getContainersPath().c_str(), 128);
            }else if(Config::getInstance().getRestoreMethod() == CHUNK_CACHE){
                cc = new ChunkCache(Config::getInstance().getContainersPath().c_str(), 128);
            }
            
            for(auto &x : file_recipe){
                SHA1FP fp;
                memcpy(&fp, x.data(), sizeof(SHA1FP));
                LookupResult res = GlobalMetadataManagerPtr->dedupLookup(fp);

                if(res == Unique){
                    printf("Fatal error!!!\n");
                    exit(-1);
                }

                ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(fp);
                std::string ck_data = cc->getChunkData(ev);
                if(ck_data.size() != ev.chunk_length){
                    printf("Fatal error size different!!!\n");
                    exit(-1);
                }

                if(write_buffer_offset + ck_data.size() >= FILE_CACHE){
                    flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
                    write_buffer_offset = 0;
                }

                memcpy(assembling_buffer + write_buffer_offset, 
                    ck_data.data(), ck_data.size());
                write_buffer_offset += ev.chunk_length;

                restore_size += ev.chunk_length;
            }

            flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
            close(fd);
        }else if(Config::getInstance().getRestoreMethod() == FAA_FIXED){
            int fd = open(Config::getInstance().getRestorePath().c_str(), O_RDWR | O_CREAT, 0777);
            if(fd < 0){
                printf("无法写文件!!! %s\n", strerror(errno));
                exit(-1);
            }

            unsigned char* container_read_buffer = (unsigned char*)malloc(CONTAINER_SIZE);
            memset(container_read_buffer, 0, CONTAINER_SIZE);
            int buffered_CID = -1;

            int recipe_offset = 0;
            std::vector<recipe_buffer_entry> recipe_buffer;
            int write_length_from_recipe_buffer = 0;
            int faa_start = 0;

            while(recipe_offset <= file_recipe.size()-1){
                // 1.从文件recipe取一段截到recipe buffer
                write_length_from_recipe_buffer = 0;
                faa_start = 0;
                recipe_buffer.clear();
                for(; recipe_offset<=file_recipe.size()-1; recipe_offset++){
                    SHA1FP fp;
                    memcpy(&fp, file_recipe[recipe_offset].data(), sizeof(SHA1FP));
                    ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(fp);

                    if(write_length_from_recipe_buffer + ev.chunk_length <= FILE_CACHE){
                        recipe_buffer.emplace_back(
                            recipe_buffer_entry(ev.container_number, ev.chunk_length, ev.offset, faa_start, false));
                        faa_start += ev.chunk_length;
                        write_length_from_recipe_buffer += ev.chunk_length;
                    }else{
                        break;  
                    }
                }
                
                // 2.根据recipe buffer填充assembling buffer
                while(1){
                    int flag = 0;
                    for(auto &x : recipe_buffer){
                        if(!x.used){
                            buffered_CID = x.CID;
                            FAA::loadContainer(buffered_CID, Config::getInstance().getContainersPath(), container_read_buffer);
                            flag = 1;
                            break;
                        }
                    }
                    if(flag == 0) break;

                    for(auto &x : recipe_buffer){
                        if(!x.used){
                            if(x.CID == buffered_CID){
                                memcpy(assembling_buffer + x.faa_start, 
                                container_read_buffer + x.container_offset, x.length);
                                x.used = true;

                                restore_size += x.length;
                            }
                        }
                    }
                }

                // 3.当前recipe buffer已使用完，将faa刷入磁盘
                flushAssemblingBuffer(fd, assembling_buffer, write_length_from_recipe_buffer);
            }

            close(fd);
        }else{
            printf("暂不支持的恢复算法 %d\n", Config::getInstance().getRestoreMethod());
            exit(-1);
        }

        printf("-----------------------Restore statics----------------------\n");
        printf("Restore size %d\n", restore_size);

    }else if(Config::getInstance().getTaskType() == TASK_DELETE){
        // 根据 json 文件中的 fileRecipesPath 和 RestoreVersion 选择要删除的文件

        if(!fileRecipeExist(Config::getInstance().getRestoreVersion(),
                            Config::getInstance().getFileRecipesPath().c_str())){
            printf("Version %d not exist!\n", Config::getInstance().getRestoreVersion());
            return 0;
        }
        std::vector<std::string> file_recipe = getFileRecipe(Config::getInstance().getRestoreVersion(),
                                                             Config::getInstance().getFileRecipesPath().c_str());
        

        for(auto &x : file_recipe){
                SHA1FP fp;
                memcpy(&fp, x.data(), sizeof(SHA1FP));
                LookupResult res = GlobalMetadataManagerPtr->dedupLookup(fp);

                if(res == Unique){
                    printf("Fatal error!!!\n");
                    exit(-1);
                }

                ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(fp);

                int dv = GlobalMetadataManagerPtr->decRefCnt(fp);

                if(dv == 0){
                    container_del_chunk(ev.container_number, fp, ev.offset, ev.chunk_length);
                }
        }

        std::string abs_file = getRecipeNameFromVersion(Config::getInstance().getRestoreVersion(),
                                        Config::getInstance().getFileRecipesPath().c_str());
        remove(abs_file.c_str());

        GlobalMetadataManagerPtr->save();


    }

    gettimeofday(&t1, NULL);
    uint64_t single_dedup_time_us = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
    if(Config::getInstance().getTaskType() == TASK_WRITE)
        printf("Backup duration:%lu us\n", single_dedup_time_us);
    else if(Config::getInstance().getTaskType() == TASK_RESTORE)
        printf("Restore duration:%lu us\n", single_dedup_time_us);
    else if(Config::getInstance().getTaskType() == TASK_DELETE)
        printf("Delete duration:%lu us\n", single_dedup_time_us);

    
    return 0;
}