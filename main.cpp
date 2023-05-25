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


int (*chunking) (unsigned char*p, int n);

bool streamCmp(const unsigned char* s1, const unsigned char* s2, int len){
    for(int i=0; i<=len-1; i++)
        if(s1[i] != s2[i])
            return false;
    return true;
}

// class chunk START ---
class Chunk{
    private:
        uint8_t fingerprint[SHA_DIGEST_LENGTH];
        uint32_t ContainerNumber;
        uint32_t ContainerInnerOffset;
        uint16_t size;
        uint16_t ContainerInnerIndex;
    public:
        Chunk(){}
        uint16_t getSize(){return size;}
        uint32_t getConNum(){return ContainerNumber;}
        uint32_t getInnerOffset(){return ContainerInnerOffset;}
        uint8_t* getFingerprint(){return fingerprint;}
        void initWithCompact(unsigned char*);
        void initWithValue(unsigned char*,uint32_t,uint32_t,uint16_t,uint16_t);
        std::string getDataFromDisk();
        void saveMetaToDisk();
        static unsigned char restore_container_buf[CONTAINER_SIZE];
        static Chunk* lookupChunkMeta(const unsigned char*);
};

unsigned char Chunk::restore_container_buf[CONTAINER_SIZE] = {0};

Chunk* Chunk::lookupChunkMeta(const unsigned char* new_hash){
    int fd = open(fingerprintsFilePath, O_RDONLY);
    if (fd < 0) {
        std::cout << "lookupChunkMeta open error!\n";
        exit(-1);
    }

    unsigned char meta_buf[META_DATA_SIZE];
    Chunk* ans = new Chunk();
    for(int i=0; ;i++){
        int n_read = read(fd, meta_buf, META_DATA_SIZE);
        if(n_read == 0) 
            return nullptr;
        if(n_read < 0){
            std::cout<< "lookupChunkMeta read error!"<<std::endl;
            exit(-1);
        } 
        if(streamCmp(meta_buf, new_hash, SHA_DIGEST_LENGTH)){
            ans->initWithCompact(meta_buf);
            return ans;
        }
    } 
    close(fd);
    return nullptr;
}

void Chunk::initWithCompact(unsigned char* meta){
    memcpy(this->fingerprint, meta, SHA_DIGEST_LENGTH);
    memcpy(&this->ContainerNumber, meta+20, 4);
    memcpy(&this->ContainerInnerOffset, meta+24, 4);
    memcpy(&this->size, meta+28, 2);
    memcpy(&this->ContainerInnerIndex, meta+30, 2);
}

void Chunk::initWithValue(unsigned char* hash, uint32_t cn, uint32_t off, uint16_t size, uint16_t innerIndex){
    memcpy(this->fingerprint, hash, SHA_DIGEST_LENGTH);
    this->ContainerNumber = cn;
    this->ContainerInnerOffset = off;
    this->size = size;
    this->ContainerInnerIndex = innerIndex;
}

void Chunk::saveMetaToDisk(){
    int fd = open(fingerprintsFilePath, O_WRONLY | O_APPEND);
    if(fd < 0){
        printf("saveMetaToDisk open error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }

    unsigned char meta_buf[META_DATA_SIZE]={0};
    memcpy(meta_buf, this->fingerprint,  SHA_DIGEST_LENGTH);
    memcpy(meta_buf+SHA_DIGEST_LENGTH, &this->ContainerNumber,  4);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+4, &this->ContainerInnerOffset,  4);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+4+4, &this->size,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+4+4+2, &this->ContainerInnerIndex,  2);

    if(write(fd, meta_buf, META_DATA_SIZE) != META_DATA_SIZE){
        printf("saveMetaToDisk write error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
}

std::string Chunk::getDataFromDisk(){
    std::string container_name(containersPath);
    container_name.append("/container");
    container_name.append(std::to_string(this->ContainerNumber));
    int fd = open(container_name.data(), O_RDONLY);
    memset(Chunk::restore_container_buf, 0, CONTAINER_SIZE);
    read(fd, Chunk::restore_container_buf, CONTAINER_SIZE); // 可能塞不满
    close(fd);
    return std::string((char*)Chunk::restore_container_buf + this->ContainerInnerOffset, this->size);
}
// ---class chunk END


uint32_t getFilesNum(char* dirPath){
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

void saveFileRecipe(std::vector<std::string> file_recipe, char* fileRecipesPath){
    int n_version = getFilesNum(fileRecipesPath);
    std::string recipe_name(fileRecipesPath);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(n_version));
    int fd = open(recipe_name.data(), O_RDWR | O_CREAT, 777);
    if(fd < 0){ 
        printf("saveFileRecipe open error, id %d, %s\n", errno, strerror(errno)); 
        exit(-1);
    }
    for(auto x : file_recipe){
        if(write(fd, x.data(), SHA_DIGEST_LENGTH) < 0)
            printf("save recipe write error\n");
    }
}

std::string getRecipeNameFromVersion(uint8_t restore_version){
    std::string recipe_name(fileRecipesPath);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(restore_version));
    return recipe_name;
}

bool fileRecipeExist(uint8_t restore_version){
    if((access(getRecipeNameFromVersion(restore_version).data(), F_OK)) != -1)    
        return true;    
 
    return false;
}

std::vector<std::string> getFileRecipe(uint8_t restore_version){
    if(!fileRecipeExist(restore_version)){
        printf("Restore version %d not exist!!!\n", restore_version); 
        exit(-1);
    }

    std::string recipe_name = getRecipeNameFromVersion(restore_version);
    int fd = open(recipe_name.data(), O_RDONLY, 777);
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

void saveContainer(int container_index, unsigned char* container_buf, unsigned int len, char* containersPath){
    std::string container_name(containersPath);
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDWR | O_CREAT, 777);
    if(write(fd, container_buf, len) != len){
        printf("saveContainer write error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
}

void saveChunkToContainer(unsigned int& container_buf_pointer, unsigned char* container_buf, 
                          uint32_t& container_index, uint32_t& container_inner_offset, uint16_t& container_inner_index,
                          int chunk_length, int file_offset, unsigned char* file_cache, void* SHA_buf){
    // flush
    if(container_buf_pointer + chunk_length >= CONTAINER_SIZE){
        saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
        memset(container_buf, 0, CONTAINER_SIZE);
        container_index++;
        container_inner_offset = 0;
        container_buf_pointer = 0;
        container_inner_index = 0;
    }

    // container buffer
    memcpy(container_buf + container_buf_pointer, 
            file_cache + file_offset, chunk_length);
}

void flushAssemblingBuffer(int fd, unsigned char* buf, int len){
    if(write(fd, buf, len) != len){
        printf("Restore, write file error!!!\n");
        exit(-1);
    }
}

int main(int argc, char** argv){
    // 参数解析
    Config::getInstance().parse_argument(argc, argv);
    
    // 一次任务的总时间
    // MFDedup并没有计算load FP-index的时间
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    
    // 超级权限
    setuid(0);

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
        else if(task_type == TASK_RESTORE){
            ffd.restoreFile(restore_full_file_id, restore_file_path);
        }
        return 0;
    }else if(chunking_method == CDC){
        fastCDC_init(arg_SIZE*1024, NC_level);

        if(NC_level == 0)
            chunking = FastCDC_without_NC;
        else if(1<=NC_level && NC_level<=3)
            chunking = FastCDC_with_NC;
        else{
            printf("Invalid NC level: %d\n", NC_level);
        }
    }else if(chunking_method == FSC){
        if(arg_SIZE == 4){
            chunking = FSC_4;
        }else if(arg_SIZE == 8){
            chunking = FSC_8;
        }else if(arg_SIZE == 16){
            chunking = FSC_16;
        }else{
            printf("Invalid fixed size, the support size is 4 or 8 or 16\n");
            return 0;
        }
    }
    
    GlobalMetadataManagerPtr = new MetadataManager(fingerprintsFilePath);
    int current_version = getFilesNum(fileRecipesPath);
    if(current_version != 0){
        GlobalMetadataManagerPtr->load();
    }

    if(task_type == TASK_WRITE){
        int fd = open(input_file_path, O_RDONLY, 777);
        if(fd < 0){
            printf("open file error, id %d, %s\n", errno, strerror(errno));
            exit(-1);
        }

        unsigned char* file_cache = (unsigned char*)malloc(FILE_CACHE);
        struct SHA1FP sha1_fp;
        unsigned char SHA_buf[SHA_DIGEST_LENGTH]={0};
        unsigned char meta_data_buf[META_DATA_SIZE]={0};
        std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹

        // metadata entry(except FP)
        uint32_t container_index = getFilesNum(containersPath);
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
        int dedup_chunks = 0;
        int sum_chunks = 0;
        int sum_size = 0;
        int dedup_size = 0;
        int hash_collision_sum = 0;

        // 梅克尔树重删 由于树内部块没参与重删，可能会降低重删率。
        // 暂不支持大于FILE CACHE大小文件的重删
        if(merkle_tree){
            std::vector<L0_node> L0_nodes;
            n_read = read(fd, file_cache, FILE_CACHE);
            while(file_offset < n_read){  
                chunk_length = chunking(file_cache + file_offset, n_read - file_offset);
                memset(SHA_buf, 0, SHA_DIGEST_LENGTH);
                SHA1(file_cache + file_offset, chunk_length, SHA_buf);
                L0_nodes.emplace_back(L0_node(file_offset, chunk_length, SHA_buf));

                file_offset += chunk_length;
                file_recipe.push_back(std::string((char*)SHA_buf, SHA_DIGEST_LENGTH));
            }

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
                        x.chunk_length, x.file_offset, file_cache, (void*)x.SHA1_hash.c_str());
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
            n_read = read(fd, file_cache, FILE_CACHE);
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
                                        chunk_length, file_offset, file_cache, SHA_buf);

                    // save chunk metadata
                    entry_value.container_number = container_index;
                    entry_value.offset = container_inner_offset;
                    entry_value.chunk_length = chunk_length;
                    entry_value.container_inner_index = container_inner_index;
                    GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value);

                    // 
                    container_inner_offset += chunk_length;
                    container_buf_pointer += chunk_length;
                    container_inner_index ++;

                }else if(lookup_result == Dedup){  
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
            saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
        
        // flush file_recipe
        saveFileRecipe(file_recipe, fileRecipesPath);

        // save metadata entry
        GlobalMetadataManagerPtr->save();

        // 写文件 - 重删统计
        printf("-----------------------Dedup statics----------------------\n");
        printf("Hash collision num %d\n", hash_collision_sum); // should be zero
        printf("Sum chunks num %d\n",     sum_chunks);
        printf("Sum data size %d\n",      sum_size);
        printf("Dedup chunks num %d\n",   dedup_chunks);
        printf("Dedup data size %d\n",    dedup_size);
        printf("Dedup Ratio %.2f%\n",     double(dedup_chunks) / sum_chunks *100);

    }else if(task_type == TASK_RESTORE){
        int restore_size = 0;

        if(!fileRecipeExist(restore_version)){
            printf("Version %d not exist!\n", restore_version);
            return 0;
        }
        
        unsigned char* assembling_buffer = (unsigned char*)malloc(FILE_CACHE);
        memset(assembling_buffer, 0, FILE_CACHE);
        int write_buffer_offset = 0;

        //recipe
        std::vector<std::string> file_recipe = getFileRecipe(restore_version);

        //组装
        if(restore_method == NAIVE_RESTORE){
            for(auto &x : file_recipe){
                Chunk *ck = Chunk::lookupChunkMeta((const unsigned char*)x.data());
                if(ck == nullptr){
                    printf("Fatal error!!!\n");
                    exit(-1);
                }

                std::string ck_data = ck->getDataFromDisk();
                if(ck_data.size() != ck->getSize()){
                    printf("Fatal error size different!!!\n");
                    exit(-1);
                }
                memcpy(assembling_buffer + write_buffer_offset, 
                    ck_data.data(), ck_data.size());
                write_buffer_offset += ck_data.size();
                delete ck;
            }   
        }else if(restore_method == CONTAINER_CACHE || restore_method == CHUNK_CACHE){
            int fd = open(restore_file_path, O_RDWR | O_CREAT, 777);
            if(fd < 0){
                printf("无法写文件!!! %s\n", strerror(errno));
                exit(-1);
            }

            Cache* cc;
            if(restore_method == CONTAINER_CACHE){
                cc = new ContainerCache(containersPath, 128);
            }else if(restore_method == CHUNK_CACHE){
                cc = new ChunkCache(containersPath, 128);
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
        }else if(restore_method == FAA_FIXED){
            int fd = open(restore_file_path, O_RDWR | O_CREAT, 777);
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
                            FAA::loadContainer(buffered_CID, containersPath, container_read_buffer);
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
            printf("暂不支持的恢复算法 %d\n", restore_method);
            exit(-1);
        }

        printf("-----------------------Restore statics----------------------\n");
        printf("Restore size %d\n", restore_size);
    }

    gettimeofday(&t1, NULL);
    uint64_t single_dedup_time_us = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
    if(task_type == TASK_WRITE)
        printf("Backup duration:%lu us\n", single_dedup_time_us);
    else if(task_type == TASK_RESTORE)
        printf("Restore duration:%lu us\n", single_dedup_time_us);
    
    return 0;
}