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
#include "compressor.h"
#include "global_stat.h"
#include "jcr.h"
#include "pipeline.h"

#define MB (1024*1024)

uint32_t rev_container_cnt = 0;
long long write_containers_num = 0;
unsigned char rev_container_buf[CONTAINER_SIZE]={0};
unsigned char tmp_buf[CONTAINER_SIZE]={0};
char* global_stat_path = "/home/cyf/cDedup/global_status.json";
extern MetadataManager *GlobalMetadataManagerPtr;

ssize_t (*chunking) (unsigned char*p, int n);

uint32_t getFilesNum(const char* dirPath){
    int ans = 0;
    DIR *dir = opendir(dirPath);
    if(!dir){
        printf("getFilesNum opendir error, id %d, %s, the dir is %s\n", 
        errno, strerror(errno), dirPath);
        closedir(dir);
        exit(-1);
    }

    while(readdir(dir)) ans++;
    closedir(dir);
    return ans-2;
}

void saveFileRecipe(std::vector<std::string> file_recipe, const char* fileRecipesPath, int version){
    std::string recipe_name(fileRecipesPath);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(version));
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

void saveFileRecipeBatch(const uint8_t* file_recipe_cache, int fp_num, const char* fileRecipesPath, int version){
    std::string recipe_name(fileRecipesPath);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(version));
    int fd = open(recipe_name.data(), O_RDWR | O_CREAT, 0777);
    if(fd < 0){ 
        printf("saveFileRecipe open error, id %d, %s\n", errno, strerror(errno)); 
        exit(-1);
    }
    
    write(fd, file_recipe_cache, fp_num*SHA_DIGEST_LENGTH);

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

void writeContainer(unsigned char* container_buf, unsigned int len, const char* container_pool_path, int container_index){
    FILE* file = fopen(container_pool_path, "a");
    fseek(file, container_index * CONTAINER_SIZE, SEEK_SET);

    if(fwrite(container_buf, CONTAINER_SIZE, 1, file) != 1){
        perror("Fail to append a container in ContainerPool.");
        exit(1);
	}

    fclose(file);
}

void saveChunkToContainer(unsigned int& container_buf_pointer, unsigned char* container_buf, 
                          uint32_t& container_index, uint32_t& container_inner_offset, 
                          int chunk_length, uint16_t& container_inner_index,
                          int file_offset, unsigned char* file_cache,
                          const char* containers_path){
    // flush
    if(container_buf_pointer + chunk_length >= CONTAINER_SIZE){
        writeContainer(container_buf, container_buf_pointer, containers_path, container_index);
        memset(container_buf, 0, CONTAINER_SIZE);
        container_index++;
        container_inner_offset = 0;
        container_buf_pointer = 0;
        container_inner_index = 0;
        write_containers_num++;
    }

    // container buffer
    memcpy(container_buf + container_buf_pointer, file_cache + file_offset, chunk_length);
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
        Equaler te;
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

    // 全局重删统计解析
    GlobalStat::getInstance().parse_arguments(global_stat_path);
    
    // chunking method
    if(Config::getInstance().getChunkingMethod() == FastCDC){
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
        }else if(avg_size == 512){
            chunking = FSC_512;
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

    if(Config::getInstance().getTaskType() == TASK_WRITE_PIPELINE){
        //
        init_backup_jcr();
        
        //total time start
        struct timeval backup_time_start, backup_time_end;
        gettimeofday(&backup_time_start, NULL);

        //start phases
        start_read_phase();
		start_chunk_phase();
		start_hash_phase();
        start_dedup_phase();
        
        // wait for the last pipeline finish
        while(jcr.status == JCR_STATUS_RUNNING || jcr.status != JCR_STATUS_DONE){
            usleep(10);
        }

        //stop phases
        stop_read_phase();
		stop_chunk_phase();
		stop_hash_phase();
        stop_dedup_phase();
        
        //total time end
        gettimeofday(&backup_time_end, NULL);
        jcr.total_time = (backup_time_end.tv_sec - backup_time_start.tv_sec) * 1000000 + 
                          backup_time_end.tv_usec - backup_time_start.tv_usec;
        
        // statistcs
        show_backup_jcr();

    }else if(Config::getInstance().getTaskType() == TASK_WRITE){
        struct timeval backup_time_start, backup_time_end;
        gettimeofday(&backup_time_start, NULL);

        int idf = open(Config::getInstance().getInputPath().c_str(), O_RDONLY, 0777);
        if(idf < 0){
            printf("open file error, id %d, %s, %s\n", errno, strerror(errno), Config::getInstance().getInputPath().c_str());
            exit(-1);
        }

        struct SHA1FP *sha1_fp = (struct SHA1FP *)malloc(sizeof(SHA1FP));
        struct ENTRY_VALUE *entry_value = (struct ENTRY_VALUE *)malloc(sizeof(ENTRY_VALUE));
        
        std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹
        uint8_t* file_recipe_cache = (uint8_t*)malloc(128*MB);
        int file_recipe_cache_off = 0;

        // metadata entry(except FP)
        uint32_t container_index = GlobalStat::getInstance().getContainersNum();
        uint32_t container_inner_offset = 0;
        uint32_t chunk_length = 0;
        uint16_t container_inner_index = 0;

        unsigned char container_buf[CONTAINER_SIZE]={0};
        unsigned int container_buf_pointer = 0;
        
        unsigned char* file_cache = (unsigned char*)malloc(FILE_CACHE);
        ssize_t file_offset = 0;
        ssize_t n_read = 0;

        // 重删统计
        long long dedup_chunks = 0;
        long long sum_chunks = 0;
        long long sum_size = 0;
        long long dedup_size = 0;
        
        
        // synchronous main loop
        while(1){
            // Reading a block from this file
            file_offset = 0;
            n_read = read(idf, file_cache, FILE_CACHE);

            if(n_read <= 0){
                break;
            }

            // Deduplicating this block
            while(file_offset < n_read){  
                // Chunking
                chunk_length = chunking(file_cache + file_offset, n_read - file_offset);
                
                // Hashing
                SHA1(file_cache + file_offset, chunk_length, (uint8_t*)sha1_fp);

                // Lookuping
                LookupResult lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp);

                // Insert fingerprint into file recipe
                //file_recipe.push_back(std::string((char*)sha1_fp, sizeof(struct SHA1FP)));
                memcpy(file_recipe_cache+file_recipe_cache_off, sha1_fp, 20);
                file_recipe_cache_off+=20;

                // Statistic
                sum_chunks ++;
                sum_size += chunk_length;
                
                if(lookup_result == Unique){
                    // saving chunk itself
                    saveChunkToContainer(container_buf_pointer, container_buf, 
                                        container_index, container_inner_offset, 
                                        chunk_length, container_inner_index,
                                        file_offset, file_cache, 
                                        Config::getInstance().getContainersPath().c_str());
                    
                    // saving chunk's metadata
                    entry_value->container_number = container_index;
                    entry_value->offset = container_inner_offset;
                    entry_value->chunk_length = chunk_length;
                    entry_value->container_inner_index = container_inner_index;
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value);

                    container_inner_offset += chunk_length;
                    container_buf_pointer += chunk_length;
                    container_inner_index ++;
                }else if(lookup_result == Dedup){
                    // Statistic
                    dedup_chunks ++;
                    dedup_size += chunk_length;
                }

                file_offset += chunk_length;
            }
        }
     
        // flushing the last container
        if(container_buf_pointer > 0){
            writeContainer(container_buf, 
                          container_buf_pointer, Config::getInstance().getContainersPath().c_str(), container_index);
            write_containers_num++;
        }
            
        // Backup Throughput
        gettimeofday(&backup_time_end, NULL);
        uint64_t single_dedup_time_us = (backup_time_end.tv_sec - backup_time_start.tv_sec) * 1000000 + backup_time_end.tv_usec - backup_time_start.tv_usec;
        float throughput = (float)(sum_size) / MB / ((float)(single_dedup_time_us)/1000000);

        printf("-----------------------Deduplication Statistic----------------------\n");
        printf("Sum chunks num %lld\n",     sum_chunks);
        printf("Sum data size %lld\n",      sum_size);
        printf("Dedup chunks num %lld\n",   dedup_chunks);
        printf("Dedup data size %lld\n",    dedup_size);

        printf("-----------------------Statistic----------------------\n");
        printf("Throughput %.2f MiB/s\n",    throughput);
        printf("Dedup Ratio %.2f%\n",     double(dedup_size) / double(sum_size) *100);
        printf("Average chunk size(all) %d\n", sum_size/sum_chunks);
        
        close(idf);

        // save file_recipe
        saveFileRecipeBatch(file_recipe_cache, sum_chunks, Config::getInstance().getFileRecipesPath().c_str(), current_version);

        // save metadata entry
        GlobalMetadataManagerPtr->saveBatch();
        
        // updating and saving statistic
        GlobalStat::getInstance().update_by_backup_job(sum_size, sum_size - dedup_size, 
                                sum_chunks, sum_chunks-dedup_chunks, write_containers_num);
        GlobalStat::getInstance().save_arguments(global_stat_path);

    }else if(Config::getInstance().getTaskType() == TASK_RESTORE){
        struct timeval restore_time_start, restore_time_end;
        gettimeofday(&restore_time_start, NULL);

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
                cc = new ChunkCache(Config::getInstance().getContainersPath().c_str(), 16*1024);
            }
            
            for(auto &x : file_recipe){
                SHA1FP fp;
                memcpy(&fp, x.data(), sizeof(SHA1FP));
                LookupResult res = GlobalMetadataManagerPtr->dedupLookup(&fp);

                if(res == Unique){
                    printf("Fatal error!!!\n");
                    exit(-1);
                }

                ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(&fp);
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
                    ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(&fp);

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

        gettimeofday(&restore_time_end, NULL);
        uint64_t single_dedup_time_us = (restore_time_end.tv_sec - restore_time_start.tv_sec) * 1000000 + restore_time_end.tv_usec - restore_time_start.tv_usec;
        float restore_throughput = (float)(restore_size) / MB / ((float)(single_dedup_time_us)/1000000);
        printf("-----------------------Restore statics----------------------\n");
        printf("Restore size %d\n", restore_size);
        printf("Restore Throughput %.2f MiB/s\n", restore_throughput);

    }else if(Config::getInstance().getTaskType() == TASK_DELETE){
        struct timeval delete_time_start, delete_time_end;
        gettimeofday(&delete_time_start, NULL);

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
                LookupResult res = GlobalMetadataManagerPtr->dedupLookup(&fp);

                if(res == Unique){
                    printf("Fatal error!!!\n");
                    exit(-1);
                }

                ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(&fp);

                //int dv = GlobalMetadataManagerPtr->decRefCnt(fp);

                // if(dv == 0){
                //     container_del_chunk(ev.container_number, fp, ev.offset, ev.chunk_length);
                // }
        }

        std::string abs_file = getRecipeNameFromVersion(Config::getInstance().getRestoreVersion(),
                                        Config::getInstance().getFileRecipesPath().c_str());
        remove(abs_file.c_str());

        GlobalMetadataManagerPtr->save();

        gettimeofday(&delete_time_end, NULL);
        uint64_t single_delete_time_us = (delete_time_end.tv_sec - delete_time_start.tv_sec) * 1000000 + delete_time_end.tv_usec - delete_time_start.tv_usec;
        printf("-----------------------Delete Statics----------------------\n");
        printf("Delete time %d ms\n", single_delete_time_us/1000);
    }

    
    return 0;
}