#include "deltaDedup_gc.h"
#include "config.h"
#include "recipe.h"
#include "deltaDedup_stats.h"
#include "../utils/metadata.h"
#include "MetadataManager.h"
#include "backup_job.h"
#include "container.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_set>
#include <iostream>

extern MetadataManager *GlobalMetadataManagerPtr;

void del_seg(std::string s, int oft, int len){
    int fd = open(s.c_str(), O_RDONLY, 0777);
    std::string s2 = s + "tmp";
    int fd2 = open(s2.c_str(), O_RDWR | O_CREAT, 0777);

    unsigned char tmp_buf[CONTAINER_SIZE]={0};
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

void container_del_chunk(uint32_t cid, SHA1FP del_sha, int oft, int len){
    std::string ctr_path = Config::getInstance().getContainersPath() + "/container" + std::to_string(cid);
    del_seg(ctr_path, oft, len);
    
    ctr_path.append("r");
    uint32_t rev_container_cnt;
    int fd = open(ctr_path.c_str(), O_RDONLY, 0777);
    read(fd, &rev_container_cnt, sizeof(uint32_t));
    if(rev_container_cnt == 1){
        close(fd);
        remove(ctr_path.c_str());
        return;
    }
    int d_pos = -1;
    //printf("rev_container_cnt: %d\n",rev_container_cnt);
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

void deleteFile(int delete_version,FILE_ATTR file_attr){
    string recipe_path = Config::getInstance().getFileRecipesPath();
    if(!fileRecipeExist(delete_version, recipe_path.c_str())){
        printf("Version %d not exist!\n", delete_version);
        exit(-1);
    }
    
    printf("-----------------------Begin to delete file version %d-----------------------\n",delete_version);
    struct timeval delete_time_start, delete_time_end;  
    gettimeofday(&delete_time_start, NULL);

    std::vector<std::string> file_recipe = getFileRecipe(delete_version,recipe_path.c_str());
    uint64_t file_size = 0;
    uint64_t stored_size = 0;
    std::string fp_name;
    if(Config::getInstance().getDedupMethod() != DEDUP_GLOBAL){
        SHA1FP fp;
        ENTRY_VALUE ev;
        unordered_set<ContainerKey,ContainerKeyHash> usedContainers;
        GlobalMetadataManagerPtr->loadVersion(delete_version,true);
        ContainerKey key;

        for(auto &x : file_recipe){
            memcpy(&fp, x.data(), sizeof(SHA1FP));
            ev = GlobalMetadataManagerPtr->getEntry(fp);
            file_size += ev.chunk_length;
            if(ev.version == delete_version){
                key = {ev.container_type,ev.container_number};
                usedContainers.insert(key);
                stored_size += ev.chunk_length;
            }
        }

        //移除container
        string container_path;
        string container_name;
        for(auto &cid: usedContainers){
            container_path = (cid.type == HOT_CONTAINER) ? Config::getInstance().getHotContainersPath() : Config::getInstance().getContainersPath();
            container_name = container_path + "/" + container_type_to_string(cid.type) + to_string(cid.containerId);
            if (remove(container_name.c_str()) != 0) {
                cerr << "delete container fail: " << container_name << endl;
                exit(-1);
            }
        }

        //移除fingerprint
        fp_name = GlobalMetadataManagerPtr->genFPname(delete_version, file_attr);
        remove(fp_name.c_str());
    }else{
        //全局索引重删的删除
        for(auto &x : file_recipe){
            SHA1FP fp;
            memcpy(&fp, x.data(), sizeof(SHA1FP));
            LookupResult res = GlobalMetadataManagerPtr->dedupLookup(fp);

            if(res == Unique){
                printf("Fatal error!!!\n");
                exit(-1);
            }

            ENTRY_VALUE ev = GlobalMetadataManagerPtr->getEntry(fp);
            file_size += ev.chunk_length;

            int dv = GlobalMetadataManagerPtr->decRefCnt(fp);
            if(dv == 0){
                container_del_chunk(ev.container_number, fp, ev.offset, ev.chunk_length);
                stored_size += ev.chunk_length;
            }
        }

        GlobalMetadataManagerPtr->save();
    }

    //移除recipe
    std::string recipe_name = recipe_path + "/recipe" + std::to_string(delete_version);
    remove(recipe_name.c_str());
    
    bj.sum_size = bj.sum_size - file_size;
    bj.dedup_size = bj.dedup_size - file_size + stored_size;

    gettimeofday(&delete_time_end, NULL);
    uint64_t single_delete_time_us = (delete_time_end.tv_sec - delete_time_start.tv_sec) * 1000000 + 
                                        delete_time_end.tv_usec - delete_time_start.tv_usec;
    printf("Delete time %.2f s\n",(float)(single_delete_time_us)/1000000);
}


// DeltaDedup-保留最近n个版本的删除（全局重删的删除还没有）
void do_delete(int current_version){
    int retain_version_number = Config::getInstance().getRetainVersionNumber();
    if(retain_version_number < 0 || current_version < retain_version_number)
        return ;
    
    int delete_version = current_version - Config::getInstance().getRetainVersionNumber();

    // DeltaDedup GC
    if(Config::getInstance().getDedupMethod() != DEDUP_GLOBAL){
        vector<AttrWithDR> dr_vec = loadAllDedupRatios();
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
    else{
        deleteFile(delete_version, ATTR_BASE); //后面的属性无用
    }   
    
}