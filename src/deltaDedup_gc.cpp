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

void deleteFile(int delete_version,bool in_delta){
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

        fp_name = GlobalMetadataManagerPtr->genFPname(delete_version, !in_delta);
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

        // DeltaDedup和普通删除不同
        if(Config::getInstance().getDedupMethod() != DEDUP_GLOBAL){
            // uint32_t base_size = Config::getInstance().getBaseSize();
            // uint32_t delta_num = Config::getInstance().getDeltaNum();
            // bool in_delta = (delete_version % (base_size + delta_num)) > (base_size-1);
            vector<pair<string, double>> dr_vec = loadAllDedupRatios();
            auto [attr, dr] = dr_vec.at(delete_version);
            bool in_delta = (attr == "delta");
            
            if(in_delta){
                deleteFile(delete_version, true);
                //如果连续删除，删掉最后一个delta版本之后，删除该版本对应的base
                // if(delete_version % (base_size+delta_num) == delta_num){
                //     deleteFile(delete_version-delta_num, false);
                // }
                if(delete_version+1 < dr_vec.size() && dr_vec.at(delete_version+1).first == "base"){
                    //deleteFile(delete_version-1,false);
                    for (int i = delete_version - 1; i >= 0; --i) {
                        if (dr_vec[i].first == "base") {
                            deleteFile(i,false); //删除对应的base
                            break;
                        }
                    }
                }
            }
        }else{
            //deleteFile(delete_version,true);
        }
        
    }
}