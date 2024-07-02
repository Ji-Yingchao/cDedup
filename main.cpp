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
#include <experimental/filesystem>
#include <algorithm>

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
#define GB (1024*1024*1024)

struct backup_job{
    uint64_t hash_collision_sum;
    uint64_t sum_chunks;
    uint64_t sum_size;
    uint64_t dedup_chunks;
    uint64_t dedup_size;
    uint64_t file_num;
};

namespace fs = std::experimental::filesystem;

uint32_t rev_container_cnt = 0;
unsigned char rev_container_buf[CONTAINER_SIZE]={0};
unsigned char tmp_buf[CONTAINER_SIZE]={0};

char* global_stat_path = "/home/jyc/cDedup/global_stat.json";
extern MetadataManager *GlobalMetadataManagerPtr;
int (*chunking) (unsigned char*p, int n);
struct backup_job bj;

bool clear_base = true;

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

}

void flushAssemblingBuffer(int fd, unsigned char* buf, int len){
    if(write(fd, buf, len) != len){
        printf("Restore, write file error!!!\n");
        exit(-1);
    }
    //close(fd);
}

void initChunkingAlgorithm(){
    if(Config::getInstance().getChunkingMethod() == CDC){
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
            exit(-1);
        }
    }
}

void writeFile(string path){
    int idf = open(path.c_str(), O_RDONLY, 0777);
    if(idf < 0){
        printf("open file error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }

    unsigned char* file_cache = (unsigned char*)malloc(FILE_CACHE);
    struct SHA1FP sha1_fp;
    std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹

    // metadata entry(except FP)
    uint32_t container_index = getFilesNum(Config::getInstance().getContainersPath().c_str());
    uint32_t container_inner_offset = 0;
    uint32_t chunk_length = 0;
    uint16_t container_inner_index = 0;

    unsigned char container_buf[CONTAINER_SIZE]={0};
    unsigned int container_buf_pointer = 0;
    uint32_t file_offset = 0;
    uint32_t n_read = 0;

    struct ENTRY_VALUE entry_value;

    // 重删统计
    uint64_t dedup_chunks = 0;
    uint64_t dedup_size = 0;
    uint64_t sum_chunks = 0;
    uint64_t sum_size = 0;
    uint64_t hash_collision_sum = 0;

    // delta重删
    bool dd = Config::getInstance().isDeltaDedup();
    uint32_t current_version = getFilesNum(Config::getInstance().getFileRecipesPath().c_str());
    uint32_t base_size = Config::getInstance().getBaseSize();
    uint32_t delta_num = Config::getInstance().getDeltaNum();
    uint32_t min_dr = Config::getInstance().getMinDR();
    uint32_t min_destination_base = current_version -  current_version % (base_size + delta_num);
    uint32_t max_destination_base = min_destination_base + base_size - 1;
    bool in_delta = false;
    if(min_dr == 0){
        in_delta = (current_version % (base_size + delta_num)) > (base_size-1);
    }else if(clear_base){
        in_delta = false;
    }else{
        in_delta = true;
    }
    
    // 普通分块重删，来一个块查寻一次，然后把non-duplicate chunk保存到container去
    for(;;){
        file_offset = 0;

        n_read = read(idf, file_cache, FILE_CACHE);

        if(n_read <= 0){
            break;
        }

        while(file_offset < n_read){  
            // Chunk
            chunk_length = chunking(file_cache + file_offset, n_read - file_offset);
            
            // Hash
            memset(&sha1_fp, 0, sizeof(struct SHA1FP));
            SHA1(file_cache + file_offset, chunk_length, (uint8_t*)&sha1_fp);

            // Dedup
            LookupResult lookup_result;
            
            if(dd)
                lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp, in_delta); 
            else
                lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp);

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
                entry_value.version = current_version;
                entry_value.ref_cnt = 1;

                if(dd){
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value, in_delta);
                }else{
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value);
                }

                // rev
                container_inner_offset += chunk_length;
                container_buf_pointer += chunk_length;
                container_inner_index ++;
                rev_container_cnt ++;

            }else if(lookup_result == Dedup){
                dedup_chunks ++;
                dedup_size += chunk_length;
            }

            // Insert fingerprint into file recipe
            file_recipe.push_back(std::string((char*)&sha1_fp, sizeof(struct SHA1FP)));

            // Statistic
            sum_chunks ++;
            sum_size += chunk_length;

            file_offset += chunk_length;
        }
    }

    //  flush最后一个container
    if(container_buf_pointer > 0)
        saveContainer(container_index, container_buf, 
                        container_buf_pointer, Config::getInstance().getContainersPath().c_str());
    
    // flush file_recipe
    saveFileRecipe(file_recipe, Config::getInstance().getFileRecipesPath().c_str());
    // printf("Sum chunks %" PRIu64 "\n",    sum_chunks);
    // printf("Sum size %" PRIu64 "\n",    sum_size);
    // printf("Unique chunks %" PRIu64 "\n",    sum_chunks - dedup_chunks);
    double cur_dr = double(dedup_size) / double(sum_size);
    printf("%.2f%\n",     double(dedup_size) / double(sum_size) *100);

    // update backup job
    bj.dedup_chunks += dedup_chunks;
    bj.dedup_size += dedup_size;
    bj.sum_chunks += sum_chunks;
    bj.sum_size += sum_size;
    bj.hash_collision_sum += hash_collision_sum;
    bj.file_num++;

    if(dd && (min_dr == 0)){
        GlobalMetadataManagerPtr->save(current_version, delta_num, min_destination_base);
    }else if(dd && (min_dr > 0)){
        clear_base = (cur_dr < (double)min_dr/100) && in_delta;
        GlobalMetadataManagerPtr->saveVersion(current_version, in_delta, clear_base);
    }

    // free 
    close(idf);
    free(file_cache);
}

std::vector<fs::path> traverseDirectory(const fs::path& directory) {
    try {
        std::vector<fs::path> files;

        // 遍历目录
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (fs::is_regular_file(entry)) {
                files.push_back(entry.path());
            } else if (fs::is_directory(entry)) {
                traverseDirectory(entry.path());
            }
        }

        return files;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}

void traverseWriteDirectory(const fs::path& directory) {
    try {
        // 遍历目录
        std::vector<fs::path> files = traverseDirectory(directory);

        // 对文件名进行排序
        std::sort(files.begin(), files.end());

        for (const auto& path : files){
            //cout << path <<endl;
            writeFile(path);
        }
            
        
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
    }
}

int main(int argc, char** argv){
    // 超级权限
    setuid(0);

    // 参数解析
    Config::getInstance().parse_argument(argc, argv);

    // 全局统计信息解析
    GlobalStat::getInstance().parse_arguments(global_stat_path);
    
    GlobalMetadataManagerPtr = new MetadataManager(Config::getInstance().getFingerprintsFilePath().c_str());
    
    // 不支持普通重删和DeltaDedup混合写入
    if(Config::getInstance().getTaskType() == TASK_WRITE){
        int current_version = getFilesNum(Config::getInstance().getFileRecipesPath().c_str());
        if(current_version != 0){
            if(!Config::getInstance().isDeltaDedup())
                GlobalMetadataManagerPtr->load();
        }

        initChunkingAlgorithm();

        string input_path = Config::getInstance().getInputPath();

        if (!fs::exists(input_path)) {
            std::cerr << "Error: Input path does not exist." << std::endl;
            return 1;
        }

        struct timeval backup_time_start, backup_time_end;
        gettimeofday(&backup_time_start, NULL);

        if (!fs::is_directory(input_path)) {
            writeFile(input_path);
        }else{
            traverseWriteDirectory(input_path);
        }

        gettimeofday(&backup_time_end, NULL);

        // throughput
        uint64_t single_dedup_time_us = (backup_time_end.tv_sec - backup_time_start.tv_sec) * 1000000 + 
                                         backup_time_end.tv_usec - backup_time_start.tv_usec;
        float throughput = (float)(bj.sum_size) / MB / ((float)(single_dedup_time_us)/1000000);

        // 写文件 - 重删统计
        printf("-----------------------Dedup statics----------------------\n");
        printf("Hash collision num %" PRIu64 "\n",    bj.hash_collision_sum); // should be zero
        printf("Sum chunks num % " PRIu64 "\n",       bj.sum_chunks);
        printf("Sum data size %" PRIu64 "\n",         bj.sum_size);
        printf("Average chunk size %" PRIu64 "\n",    bj.sum_size / bj.sum_chunks);
        printf("Dedup chunks num %" PRIu64 "\n",      bj.dedup_chunks);
        printf("Dedup data size %" PRIu64 "\n",       bj.dedup_size);
        printf("-----------------------statics----------------------\n");
        printf("Throughput %.2f MiB/s\n",    throughput);
        printf("Dedup Ratio %.2f%\n",     double(bj.dedup_size) / double(bj.sum_size) *100);

        // 保存全局信息
        GlobalStat::getInstance().update(bj.sum_size, bj.sum_size - bj.dedup_size);
        GlobalStat::getInstance().save_arguments(global_stat_path);
        
        // save metadata entry
        GlobalMetadataManagerPtr->save();

    }else if(Config::getInstance().getTaskType() == TASK_RESTORE){
        // 如果写时使用DeltaDedup，那么恢复时参数也需要指定DeltaDedup
        int restore_version = Config::getInstance().getRestoreVersion();
        string recipe_path = Config::getInstance().getFileRecipesPath();

        int current_version = getFilesNum(recipe_path.c_str());
        if(current_version != 0){
            if(Config::getInstance().isDeltaDedup()){
                if(Config::getInstance().getMinDR() == 0){
                    GlobalMetadataManagerPtr->load(restore_version);
                }else{
                    GlobalMetadataManagerPtr->loadVersion(restore_version);
                }
            }else{
                GlobalMetadataManagerPtr->load();
            }
        }

        struct timeval restore_time_start, restore_time_end;
        gettimeofday(&restore_time_start, NULL);

        if(!fileRecipeExist(restore_version, recipe_path.c_str())){
            printf("Version %d not exist!\n", restore_version);
            return 0;
        }
        
        unsigned char* assembling_buffer = (unsigned char*)malloc(FILE_CACHE);

        memset(assembling_buffer, 0, FILE_CACHE);
        int write_buffer_offset = 0;
        uint64_t restored_size = 0;
        uint64_t container_read_count = 0;

        //recipe
        std::vector<std::string> file_recipe = getFileRecipe(Config::getInstance().getRestoreVersion(),
                                                             Config::getInstance().getFileRecipesPath().c_str());

        //组装
        RESTORE_METHOD rm = Config::getInstance().getRestoreMethod();
        if(rm == CONTAINER_CACHE || rm == CHUNK_CACHE){
            int fd = open(Config::getInstance().getRestorePath().c_str(), O_RDWR | O_CREAT, 0777);
            if(fd < 0){
                printf("无法写文件!!! %s\n", strerror(errno));
                exit(-1);
            }

            Cache* cc;
            if(rm == CONTAINER_CACHE){
                cc = new ContainerCache(Config::getInstance().getContainersPath().c_str(), Config::getInstance().getCacheSize());
            }else if(rm == CHUNK_CACHE){
                cc = new ChunkCache(Config::getInstance().getContainersPath().c_str(), 16*1024);
            }
            
            SHA1FP fp;
            ENTRY_VALUE ev;
            for(auto &x : file_recipe){
                memcpy(&fp, x.data(), sizeof(SHA1FP));
                ev = GlobalMetadataManagerPtr->getEntry(fp);
                std::string ck_data = cc->getChunkData(ev);

                if(write_buffer_offset + ck_data.size() >= FILE_CACHE){
                    flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
                    write_buffer_offset = 0;
                }

                memcpy(assembling_buffer + write_buffer_offset, ck_data.data(), ck_data.size());

                write_buffer_offset += ev.chunk_length;
                restored_size += ev.chunk_length;
            }

            flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
            container_read_count = cc->getContainerReadCount();
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

                                restored_size += x.length;
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
        float restore_throughput = (float)(restored_size) / MB / ((float)(single_dedup_time_us)/1000000);
        float speed_factor = (float)(restored_size) / MB / ((float)container_read_count);
        printf("-----------------------Restore statics----------------------\n");
        printf("Restore size %" PRIu64 "\n",    restored_size);
        printf("Restore Throughput %.2f MiB/s\n", restore_throughput);
        printf("Speed factor %.2f\n", speed_factor);
    }

    return 0;
}