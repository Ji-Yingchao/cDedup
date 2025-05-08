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
#include <regex>
#include <sys/sendfile.h>
#include <experimental/filesystem>
#include <algorithm>

#include "fastcdc.h"
#include "MetadataManager.h"
#include "ContainerCache.h"
#include "ChunkCache.h"
#include "IndependentCache.h"
#include "general.h"
#include "FAA.h"
#include "config.h"
#include "recipe.h"
#include "container.h"
#include "deltaDedup_stats.h"
#include "deltaDedup_gc.h"
#include "OutputContainer.h"
#include "backup_job.h"
#include "utils/metadata.h"
#include "utils/cJSON.h"
#include "global_stat.h"
#include "jcr.h"
#include "pipeline.h"

namespace fs = experimental::filesystem;

uint32_t rev_container_cnt = 0;
unsigned char rev_container_buf[CONTAINER_SIZE]={0};
unsigned char tmp_buf[CONTAINER_SIZE]={0};

char* global_stat_path = "/home/jyc/cDedup/global_stat.json";
extern MetadataManager *GlobalMetadataManagerPtr;
int (*chunking) (unsigned char*p, int n);

void flushAssemblingBuffer(int fd, unsigned char* buf, int len){
    if(write(fd, buf, len) != len){
        printf("Restore, write file error!!!\n");
        cerr << "write failed: " << strerror(errno) << endl;
        exit(-1);
    }
    // fsync(fd);
    //close(fd);
}

void do_arrange(int current_version){
    if(!Config::getInstance().getArranged() || current_version < 1) return ;
    
    // 根据历史记录查找之前的base
    int arrange_version;
    vector<AttrWithDR> dr_vec = loadAllDedupRatios();
    arrange_version = current_version-1;
    FILE_ATTR file_attr = dr_vec.at(arrange_version).attr;
    if(file_attr != ATTR_BASE) return ;

    //根据属性设置查找之后的base(最后的delta)
    // vector<FILE_ATTR> attrs = loadDeltaAttrs();
    // if(current_version + 1 < attrs.size() && attrs.at(current_version+1) == ATTR_BASE){
    //     vector<AttrWithDR> dr_vec = loadAllDedupRatios();
    //     auto [slbase_version, base_version] = findNearestBaseBefore(dr_vec, current_version);
    //     arrange_version = base_version;
    // }
    
    printf("-------------Begin to arrange file version %d-----------\n",arrange_version);
    unordered_set<int> usedContainers;
    vector<string> file_recipe = getFileRecipe(arrange_version, Config::getInstance().getFileRecipesPath().c_str());
    ContainerCache* cc = new ContainerCache(Config::getInstance().getContainersPath().c_str(), 64);
    OutputContainer hotContainer(Config::getInstance().getHotContainersPath().c_str(), HOT_CONTAINER, arrange_version);
    OutputContainer coldContainer(Config::getInstance().getContainersPath().c_str(), COLD_CONTAINER, arrange_version);

    SHA1FP fp;  
    for(auto& x : file_recipe){
        memcpy(&fp, x.data(), sizeof(SHA1FP));
        ENTRY_VALUE& entry = GlobalMetadataManagerPtr->getEntry(fp, ATTR_BASE);

        if (entry.container_type == CONTAINER){
            usedContainers.insert(entry.container_number);

            // write to new container and update metadata
            string ck_data = cc->getChunkData(entry);
            if(entry.ref_cnt >= 2)
                hotContainer.writeChunk(ck_data, entry);
                
            else 
                coldContainer.writeChunk(ck_data,entry);
        }
    }

    //save metadata
    GlobalMetadataManagerPtr->saveVersion(arrange_version, ATTR_BASE);

    // delete used container
    for (const auto& cid : usedContainers) {
        string path = Config::getInstance().getContainersPath() + "/container" + to_string(cid);
        if (remove(path.c_str()) != 0) {
            cerr << "Failed to delete container: " << path << endl;
            exit(-1);
        }
    }
    usedContainers.clear();
    
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


vector<fs::path> traverseDirectory(const fs::path& directory) {
    try {
        vector<fs::path> files;

        // 遍历目录
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (fs::is_regular_file(entry)) {
                files.push_back(entry.path());
            } else if (fs::is_directory(entry)) {
                traverseDirectory(entry.path());
            }
        }

        return files;
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << endl;
    }
}



void writeFile(string path){
    int idf = open(path.c_str(), O_RDONLY, 0777);
    if(idf < 0){
        printf("open file error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }

    struct timeval backup_time_start, backup_time_end;  
    gettimeofday(&backup_time_start, NULL);

    unsigned char* file_cache = (unsigned char*)malloc(FILE_CACHE);
    struct SHA1FP sha1_fp;
    vector<string> file_recipe; // 保存这个文件所有块的指纹
    vector<int> refContainers;

    // metadata entry(except FP)
    uint32_t chunk_length = 0;
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
    DEDUP_METHOD dedupMethod = Config::getInstance().getDedupMethod();
    uint32_t current_version = getVersion(Config::getInstance().getFileRecipesPath().c_str(), "recipe");
    // uint32_t max_destination_base = min_destination_base + base_size - 1;
    
    FILE_ATTR file_attr = ATTR_BASE;
    if(dedupMethod == DEDUP_INTERVAL){
        uint32_t base_size = Config::getInstance().getBaseSize();
        uint32_t delta_num = Config::getInstance().getDeltaNum();
        file_attr = (current_version % (base_size + delta_num)) > (base_size-1) ? ATTR_DELTA:ATTR_BASE;

        uint32_t min_destination_base = current_version -  current_version % (base_size + delta_num);
        if(current_version == min_destination_base + delta_num)
            GlobalMetadataManagerPtr->clear_base();
    }
    else if(dedupMethod == DEDUP_AUTOMATIC && current_version != 0){    //版本0,初始值满足动态要求
        uint32_t min_dr = Config::getInstance().getMinDR(); 
        auto [attr, dr] = loadDedupRatioAtLine(current_version-1);
        bool clear_base = dr < (double)min_dr/100 && attr == ATTR_DELTA;
        if(clear_base)
            GlobalMetadataManagerPtr->clear_base();

        //清除base的fp后，下一个必是base
        file_attr = clear_base ? ATTR_BASE : ATTR_DELTA;
    }
    else if(dedupMethod == DEDUP_MANUAL){
        vector<FILE_ATTR> attrs = loadDeltaAttrs();
        file_attr = attrs.at(current_version);

        // TODO: 如果有多个small base，也需要清除之前的sbase，但是base和sbase混用
        if(current_version+1 < attrs.size() && attrs.at(current_version+1) == ATTR_BASE)
            GlobalMetadataManagerPtr->clear_base();
    }

    
    // load metadata
    if(current_version != 0){
        if(dedupMethod == DEDUP_GLOBAL){
            GlobalMetadataManagerPtr->load();
        }
        else if(file_attr != ATTR_BASE){
            GlobalMetadataManagerPtr->loadVersion(current_version-1,false);
        }
    }

    //GlobalMetadataManagerPtr->printBaseTable();

    OutputContainer outContainer(Config::getInstance().getContainersPath().c_str(), CONTAINER, current_version);

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
            
            if(dedupMethod == DEDUP_GLOBAL)
                lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp);
            else
                lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp, file_attr); 

            //ReWrite


            // Write
            if(lookup_result == Unique){
                // save chunk itself and metadata
                outContainer.writeChunk(chunk_length, file_offset, file_cache, entry_value);

                if(dedupMethod == DEDUP_GLOBAL){
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value);
                }else{
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value, file_attr);
                    
                    // TODO：log container sequence
                    // if (refContainers.empty() || container_index != refContainers.back()) {
                    //     refContainers.push_back(container_index);
                    // }
                }

            }else if(lookup_result == Dedup){
                dedup_chunks ++;
                dedup_size += chunk_length;
                
                if(dedupMethod == DEDUP_GLOBAL){
                    GlobalMetadataManagerPtr->addRefCnt(sha1_fp);
                }else{
                    int containerId = GlobalMetadataManagerPtr->addRefCntgetContainer(sha1_fp, file_attr);
                    
                    // log container sequence
                    if (refContainers.empty() || containerId != refContainers.back()) {
                        refContainers.push_back(containerId);
                    }
                }
            }

            // Insert fingerprint into file recipe
            file_recipe.push_back(string((char*)&sha1_fp, sizeof(struct SHA1FP)));

            // Statistic
            sum_chunks ++;
            sum_size += chunk_length;

            file_offset += chunk_length;
        }
    }
    
    // flush file_recipe
    saveFileRecipe(file_recipe, Config::getInstance().getFileRecipesPath().c_str());
    
    gettimeofday(&backup_time_end, NULL);
    uint64_t single_dedup_time_us = (backup_time_end.tv_sec - backup_time_start.tv_sec) * 1000000 + 
                                         backup_time_end.tv_usec - backup_time_start.tv_usec;
    
    float throughput = (float)(sum_size) / MB / ((float)(single_dedup_time_us)/1000000);
    printf("throughput(MB/s): %.2f\n",    throughput);
    
    double cur_dr = double(dedup_size) / double(sum_size);
    //printf("dedup ratio %.2f% \n",     double(dedup_size) / double(sum_size) *100);
    printf("Dedup Ratio %.2f \n",     double(sum_size) / double(sum_size-dedup_size) );


    // delta -> small base
    if(dedupMethod == DEDUP_AUTOMATIC && file_attr == ATTR_DELTA){   
        uint32_t sml_dr = Config::getInstance().getSmlDR(); 
        if(sml_dr != 0 && cur_dr < (double)sml_dr/100)
            file_attr = ATTR_SLBASE;
    }
    
    // save dedup ratio and container index sequence
    if(dedupMethod != DEDUP_GLOBAL){
        saveDedupRatio(file_attr,cur_dr);
        saveContainerIds(refContainers,current_version);
    }
    
    // update backup job
    bj.dedup_chunks += dedup_chunks;
    bj.dedup_size += dedup_size;
    bj.sum_chunks += sum_chunks;
    bj.sum_size += sum_size;
    bj.hash_collision_sum += hash_collision_sum;
    bj.file_num++;

    // save fp-entry metadata
    if(dedupMethod == DEDUP_GLOBAL){
        GlobalMetadataManagerPtr->save();
    }else{
        GlobalMetadataManagerPtr->saveVersion(current_version, file_attr);
    }

    if(dedupMethod != DEDUP_GLOBAL){
        do_delete(current_version);
        do_arrange(current_version);
    }
    
    // free 
    close(idf);
    free(file_cache);
}

void traverseWriteDirectory(const fs::path& directory) {
    try {
        // 遍历目录
        vector<fs::path> files = traverseDirectory(directory);

        // 对文件名进行排序
        sort(files.begin(), files.end());
        
        for (const auto& path : files){
            writeFile(path);

            printf("Actual DR %.4f \n", double(bj.sum_size) / double(bj.sum_size - bj.dedup_size) );
        }
            
        
    } catch (const exception& ex) {
        cerr << "Error: " << ex.what() << endl;
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
        initChunkingAlgorithm();

        string input_path = Config::getInstance().getInputPath();

        if (!fs::exists(input_path)) {
            cerr << "Error: Input path does not exist." << endl;
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
        //print_backup_job(bj);
        printf("-----------------------statics----------------------\n");
        printf("Throughput %.2f MiB/s\n",    throughput);
        //printf("Dedup Ratio %.2f%\n",     double(bj.dedup_size) / double(bj.sum_size) *100);

        // 保存全局信息
        GlobalStat::getInstance().update(bj.sum_size, bj.sum_size - bj.dedup_size);
        GlobalStat::getInstance().save_arguments(global_stat_path);

    }
    else if(Config::getInstance().getTaskType() == TASK_RESTORE){
        // 如果写时使用DeltaDedup，那么恢复时参数也需要指定DeltaDedup
        int restore_version = Config::getInstance().getRestoreVersion();
        string recipe_path = Config::getInstance().getFileRecipesPath();

        if(Config::getInstance().getDedupMethod() == DEDUP_GLOBAL){
            GlobalMetadataManagerPtr->load();
        }else{
            GlobalMetadataManagerPtr->loadVersion(restore_version,true);
        }

        //GlobalMetadataManagerPtr->printOriginTable();
        //GlobalMetadataManagerPtr->printFPRefCnt();
        int base_container_max_value = GlobalMetadataManagerPtr->getBaseContainerMaxValue();
        printf("Base Container Max Value: %d\n", base_container_max_value);


        struct timeval restore_time_start, restore_time_end;
        gettimeofday(&restore_time_start, NULL);

        // unsigned char* assembling_buffer;
        // posix_memalign((void**)&assembling_buffer, SECTOR_SIZE, FILE_CACHE);
        unsigned char* assembling_buffer = (unsigned char*)malloc(FILE_CACHE);
        memset(assembling_buffer, 0, FILE_CACHE);
        int write_buffer_offset = 0;
        uint64_t restored_size = 0;
        int container_read_count = 0;

        //recipe
        vector<string> file_recipe = getFileRecipe(restore_version, recipe_path.c_str());

        //组装
        RESTORE_METHOD rm = Config::getInstance().getRestoreMethod();
        if(rm == CONTAINER_CACHE || rm == CHUNK_CACHE || rm == INDENPENDENT_CACHE){
            // int fd = open(Config::getInstance().getRestorePath().c_str(), O_RDWR | O_CREAT| O_DIRECT, 0777);
            int fd = open(Config::getInstance().getRestorePath().c_str(), O_RDWR | O_CREAT, 0777);
            if(fd < 0){
                printf("无法写文件!!! %s\n", strerror(errno));
                exit(-1);
            }

            double total_time1 = 0.0, total_time2 = 0.0, total_time3 = 0.0;
            struct timeval start1, end1,start2, end2;
            Cache* cc;
            if(rm == CONTAINER_CACHE){
                cc = new ContainerCache(Config::getInstance().getContainersPath().c_str(), Config::getInstance().getCacheSize());
            }else if(rm == CHUNK_CACHE){
                cc = new ChunkCache(Config::getInstance().getContainersPath().c_str(), 16*1024);
            }else if(rm == INDENPENDENT_CACHE){
                cc = new IndependentCache(Config::getInstance().getContainersPath().c_str(), Config::getInstance().getCacheSize(), base_container_max_value);
            } 
            

            SHA1FP fp;
            ENTRY_VALUE ev;
            for(auto &x : file_recipe){
                memcpy(&fp, x.data(), sizeof(SHA1FP));
                ev = GlobalMetadataManagerPtr->getEntry(fp);
                gettimeofday(&start2, NULL);
                string ck_data = cc->getChunkData(ev);
                //printf("%s\n", ck_data.c_str());

                gettimeofday(&end2, NULL);
                total_time2 += (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;
                
                // 仅数容器数量，先注释掉
                if(write_buffer_offset + ck_data.size() >= FILE_CACHE){
                    flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
                    //flushAssemblingBuffer(fd, assembling_buffer, FILE_CACHE);
                    write_buffer_offset = 0;
                }
                memcpy(assembling_buffer + write_buffer_offset, ck_data.data(), ck_data.size());

                write_buffer_offset += ev.chunk_length;
                restored_size += ev.chunk_length;
            }

            flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
            close(fd);

            // 统计读容器数量和引用容器数量
            if(rm == CONTAINER_CACHE){
                container_read_count = ((ContainerCache*)cc)->getReferenceContainerCount();
               ((ContainerCache*)cc)->printContainers(base_container_max_value);
            }else if(rm == INDENPENDENT_CACHE){
                container_read_count = ((IndependentCache*)cc)->getReferenceContainerCount();
               //((IndependentCache*)cc)->printContainers(base_container_max_value);
            }
            

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
            vector<recipe_buffer_entry> recipe_buffer;
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
        printf("RestoreTime: %.6f\n",    ((float)(single_dedup_time_us)/1000000));
        printf("-----------------------Restore statics----------------------\n");
        printf("Restore size %" PRIu64 "\n",    restored_size);
        printf("Restore Throughput %.2f MiB/s\n", restore_throughput);
        printf("Speed factor %.2f\n", speed_factor);
        
        
    }else if(Config::getInstance().getTaskType() == TASK_DELETE){
        // 仅实现固定DeltaDedup的删除
        int delete_version = Config::getInstance().getDeleteVersion();

        if(Config::getInstance().getDedupMethod() == DEDUP_GLOBAL){
            printf("暂不支持DeltaDedup以外的删除方案\n");
            exit(-1);
        }

        uint32_t base_size = Config::getInstance().getBaseSize();
        uint32_t delta_num = Config::getInstance().getDeltaNum();
        FILE_ATTR attr = (delete_version % (base_size + delta_num)) > (base_size-1) ? ATTR_DELTA:ATTR_BASE;
        if(attr == ATTR_DELTA){
            deleteFile(delete_version, ATTR_DELTA);

            //如果连续删除，删掉最后一个delta版本之后，删除该版本对应的base
            if(delete_version % (base_size+delta_num) == delta_num){
                deleteFile(delete_version-delta_num, ATTR_BASE);
            }
        }

    }
    return 0;
}