#include "deltaDedup_gc.h"
#include "config.h"
#include "recipe.h"
#include "deltaDedup_stats.h"
#include "metadata.h"
#include "MetadataManager.h"
#include "backup_job.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <unistd.h>
#include <iostream>

extern MetadataManager *GlobalMetadataManagerPtr;

void deleteFile(int delete_version,FILE_ATTR file_attr){
    string recipe_path = Config::getInstance().getFileRecipesPath();
    if(!fileRecipeExist(delete_version, recipe_path.c_str())){
        printf("Version %d not exist!\n", delete_version);
        exit(-1);
    }
    
    printf("-------------Begin to delete file version %d-----------\n",delete_version);
    struct timeval delete_time_start, delete_time_end;  
    gettimeofday(&delete_time_start, NULL);

    std::vector<std::string> file_recipe = getFileRecipe(delete_version,
                                                        Config::getInstance().getFileRecipesPath().c_str());
    uint64_t file_size = 0;
    
    std::string fp_name;
    if(Config::getInstance().getDedupMethod() != DEDUP_GLOBAL){
        GlobalMetadataManagerPtr->loadVersion(delete_version,true);
        SHA1FP fp;
        ENTRY_VALUE ev;
        for(auto &x : file_recipe){
            memcpy(&fp, x.data(), sizeof(SHA1FP));
            ev = GlobalMetadataManagerPtr->getEntry(fp);
            file_size += ev.chunk_length;
        }

        fp_name = GlobalMetadataManagerPtr->genFPname(delete_version, file_attr);
        std::vector<uint32_t> ids = getContainerIds(fp_name, file_size);
        for(auto &id: ids){
            //移除container
            uint32_t container_index = id;
            std::string container_name(Config::getInstance().getContainersPath().c_str());
            container_name.append("/container");
            container_name.append(std::to_string(container_index));
            remove(container_name.c_str());  
        }
        //移除fingerprint
        remove(fp_name.c_str());
    }else{
        //普通重删的删除未实现
        fp_name = Config::getInstance().getFingerprintsFilePath().c_str();
        printf("其他方案的删除尚未实现\n");
        exit(-1);
    }

    //移除recipe
    std::string recipe_name(recipe_path);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(delete_version));
    remove(recipe_name.c_str());
    
    bj.sum_size = bj.sum_size - file_size;
    gettimeofday(&delete_time_end, NULL);

    uint64_t single_delete_time_us = (delete_time_end.tv_sec - delete_time_start.tv_sec) * 1000000 + 
                                        delete_time_end.tv_usec - delete_time_start.tv_usec;
    printf("Delete time %.2f s\n",(float)(single_delete_time_us)/1000000);
}


// DeltaDedup-保留最近n个版本的删除（全局重删的删除还没有）
void do_delete(int current_version){
    int retain_version_number = Config::getInstance().getRetainVersionNumber();
    if(retain_version_number>=0 && current_version >= retain_version_number){
        int delete_version = current_version - Config::getInstance().getRetainVersionNumber();

        // DeltaDedup GC
        if(Config::getInstance().getDedupMethod() != DEDUP_GLOBAL){
            vector<AttrWithDR> dr_vec = loadAllDedupRatios();
            //auto [attr, dr] = dr_vec.at(delete_version);
            FILE_ATTR file_attr = dr_vec.at(delete_version).attr;
            
            if(file_attr == ATTR_DELTA){
                deleteFile(delete_version, ATTR_DELTA);
            }

            //如果连续删除，删掉最后一个delta或sbase版本之后，删除该版本对应的base
            if(delete_version+1 < dr_vec.size()){
                if(dr_vec.at(delete_version+1).attr == ATTR_SLBASE){
                    auto [last_slbase_version, last_base_version]= findNearestBaseBefore(dr_vec, delete_version);
                    if(last_slbase_version != -1)
                        deleteFile(last_slbase_version, ATTR_SLBASE); //删除对应的sbase
                }
                else if(dr_vec.at(delete_version+1).attr == ATTR_BASE){
                    auto [last_slbase_version, last_base_version]= findNearestBaseBefore(dr_vec, delete_version);
                    if(last_slbase_version != -1)
                        deleteFile(last_slbase_version, ATTR_SLBASE); 
                    if(last_base_version != -1)
                        deleteFile(last_base_version, ATTR_BASE);    //删除对应的base
                }
                
            }

            // 不删除历史记录的attr和hr
        }
        // Global Index GC
        else{
            //deleteFile(delete_version,true);
        }
        
    }
}