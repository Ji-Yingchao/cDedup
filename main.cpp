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

#define FILE_CACHE (1024*1024*1024)
#define CONTAINER_SIZE (4*1024*1024) //4MB容器

char* fingerprintsFilePath = "/home/cyf/cDedup/fingerprints.meta";
char* fileRecipesPath = "/home/cyf/cDedup/FileRecipes";
char* containersPath = "/home/cyf/cDedup/Containers";
char* fullFileFingerprintsPath = "/home/cyf/cDedup/FFFP.meta";
char* fullFileStoragePath = "/home/cyf/cDedup/FULL_FILE_STORAGE";

const char* merkleMeta[] = {fingerprintsFilePath,
                        "/home/cyf/cDedup/L1.meta", // 里面只有指纹20字节
                        "/home/cyf/cDedup/L2.meta",
                        "/home/cyf/cDedup/L3.meta",
                        "/home/cyf/cDedup/L4.meta",
                        "/home/cyf/cDedup/L5.meta",
                        "/home/cyf/cDedup/L6.meta"};

int (*chunking) (unsigned char*p, int n);

enum TASK_TYPE{
    TASK_RESTORE,
    TASK_WRITE,
    NOT_CHOOSED
};

enum CHUNKING_METHOD{
    CDC,
    FSC,
    FULL_FILE
};

struct option long_options[] = {
    { "Task", required_argument, NULL, 't' },
    { "InputFile", required_argument, NULL, 'i' },
    { "RestorePath", required_argument, NULL, 'r' },
    { "RestoreVersion", required_argument, NULL, 'v' },
    { "ChunkingMethod", required_argument, NULL, 'c' },
    { "RestoreId", required_argument, NULL, 'l' },
    { "Size", required_argument, NULL, 's' }, //used for fsc size or fastcdc avg size
    { "Normal", required_argument, NULL, 'n' }, //used for fastcdc normalized level
    { "MerkleTree", required_argument, NULL, 'm' }, 
    { 0, 0, 0, 0 }
};

enum TASK_TYPE taskTypeTrans(char* s){
    if(strcmp(s, "write") == 0){
        return TASK_WRITE;
    }else if (strcmp(s, "restore") == 0){
        return TASK_RESTORE;
    }else{
        printf("Not support task type:%s\n", s);
        exit(-1);
    }
}

enum CHUNKING_METHOD cmTypeTrans(char* s){
    if(strcmp(s, "cdc") == 0){
        return CDC;
    }else if (strcmp(s, "fsc") == 0){
        return FSC;
    }else if (strcmp(s, "file") == 0){
        return FULL_FILE;
    }else{
        printf("Not support chunking method type:%s\n", s);
        exit(-1);
    }
}

bool yesNoTrans(char* s){
    if(strcmp(s, "yes") == 0){
        return true;
    }else if (strcmp(s, "no") == 0){
        return false;
    }else{
        printf("Not support yes no type:%s\n", s);
        exit(-1);
    }
}

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
        void initWithValue(unsigned char*,uint16_t,uint32_t,uint16_t,uint16_t);
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
    memcpy(&this->ContainerNumber, meta+20, 2);
    memcpy(&this->ContainerInnerOffset, meta+22, 4);
    memcpy(&this->size, meta+26, 2);
    memcpy(&this->ContainerInnerIndex, meta+28, 2);
}

void Chunk::initWithValue(unsigned char* hash, uint16_t cn, uint32_t off, uint16_t size, uint16_t innerIndex){
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
    memcpy(meta_buf+SHA_DIGEST_LENGTH, &this->ContainerNumber,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2, &this->ContainerInnerOffset,  4);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2+4, &this->size,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2+4+2, &this->ContainerInnerIndex,  2);

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

void printBytes(unsigned char* s, int len){
    for(int i=0; i<=len-1; i++)
        printf("%x", s[i]);
    printf("\n");
}

int FSC_4(unsigned char *p, int n) {
    return 4*1024;
}

int FSC_8(unsigned char *p, int n) {
    return 8*1024;
}

int FSC_16(unsigned char *p, int n) {
    return 16*1024;
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

int main(int argc, char** argv){
    // 一次任务的总时间
    struct timeval t0, t1;
    gettimeofday(&t0, NULL);
    
    setuid(0);
    enum TASK_TYPE task_type = NOT_CHOOSED;
    enum CHUNKING_METHOD chunking_method = CDC;
    char* input_file_path = nullptr;
    char* restore_file_path = nullptr;
    uint8_t restore_version = -1;
    int restore_full_file_id = -1;
    int arg_SIZE = 4; // unit K
    int NC_level = 0;
    bool merkle_tree = false;

    int c;
    int option_index = 0;
    // CDC
    // FSC
    // FULL FILE: task type, input file path, chunking method
    while ((c = getopt_long(argc, argv, "t:i:r:v:c:l", long_options, &option_index)) != -1) {
        switch (c) {
            case 't':
                task_type = taskTypeTrans(optarg);
                break;
            case 'i':
                input_file_path = optarg;
                break;
            case 'r':
                restore_file_path = optarg;
                break;
            case 'v':
                restore_version = atoi(optarg);
                break;
            case 'c':
                chunking_method = cmTypeTrans(optarg);
                break;
            case 'l':
                restore_full_file_id = atoi(optarg);
                break;
            case 's':
                arg_SIZE = atoi(optarg);
                break;
            case 'n':
                NC_level = atoi(optarg);
                break;
            case 'm':
                merkle_tree = yesNoTrans(optarg);
                break;
            default:
                printf("Not support option: %c\n", c);
                exit(-1);
                break;
        }
    }

    if(chunking_method == FULL_FILE){
        FullFileDeduplicater ffd(fullFileStoragePath, fullFileFingerprintsPath);
        if(task_type == TASK_WRITE){
            ffd.writeFile(input_file_path);

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
    }

    if(chunking_method == CDC){
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

    if(task_type == TASK_WRITE){
        GlobalMetadataManagerPtr = new MetadataManager(fingerprintsFilePath);

        int current_version = getFilesNum(fileRecipesPath);
        
        if(current_version != 0){
            GlobalMetadataManagerPtr->load();
        }

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
        uint32_t n_read = read(fd, file_cache, FILE_CACHE);
        uint32_t file_offset = 0;
        
        MerkleTree *mt = nullptr;
        struct ENTRY_VALUE entry_value;

        // 重删统计
        int dedup_chunks = 0;
        int sum_chunks = 0;
        int sum_size = 0;
        int dedup_size = 0;
        int hash_collision_sum = 0;

        if(n_read < 0){
            printf("read file error, id %d, %s\n", errno, strerror(errno));
            exit(-1);
        }
        
        // 梅克尔树重删
        // 由于树内部块没参与重删，可能会降低重删率。
        if(merkle_tree){
            std::vector<L0_node> L0_nodes;
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
                
            }else if(lookup_result == Dedup){  
                // 重删统计
                dedup_chunks ++;
                dedup_size += chunk_length;

                // 可能遇到哈希碰撞
                // TODO
            }
            file_offset += chunk_length;
        }

    writend:       
        //  flush最后一个container
        if(container_buf_pointer > 0)
            saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
        
        // flush file_recipe
        saveFileRecipe(file_recipe, fileRecipesPath);

        // 写文件 - 重删统计
        printf("-- Dedup statistics -- \n");
        printf("Hash collision num %d\n", hash_collision_sum); // should be zero
        printf("Sum chunks num %d\n",     sum_chunks);
        printf("Sum data size %d\n",      sum_size);
        printf("Dedup chunks num %d\n",   dedup_chunks);
        printf("Dedup data size %d\n",    dedup_size);
        printf("Dedup Ratio %.2f%\n",     double(dedup_chunks) / sum_chunks *100);

    }else if(task_type == TASK_RESTORE){
        if(!fileRecipeExist(restore_version)){
            printf("Version %d not exist!\n", restore_version);
            return 0;
        }

        unsigned char* restore_file_cache = (unsigned char*)malloc(FILE_CACHE);
        memset(restore_file_cache, 0, FILE_CACHE);
        int file_cache_offset = 0;
        
        //处理recipe
        std::vector<std::string> file_recipe = getFileRecipe(restore_version);
        for(auto &x : file_recipe){
            Chunk *ck = Chunk::lookupChunkMeta((const unsigned char*)x.data());
            if(ck == nullptr){
                printf("Fatal error!!!\n");
                exit(-1);
            }
            printf("%d %d\n", ck->getConNum(), ck->getInnerOffset());
            std::string ck_data = ck->getDataFromDisk();
            if(ck_data.size() != ck->getSize()){
                printf("Fatal error size different!!!\n");
                exit(-1);
            }
            memcpy(restore_file_cache + file_cache_offset, 
                ck_data.data(), ck_data.size());
            file_cache_offset += ck_data.size();
            delete ck;
        }
        printf("file_recipe size = %ld\n", file_recipe.size());
        printf("file_cache_offset = %d\n", file_cache_offset);
        //最后写文件
        int fd = open(restore_file_path, O_RDWR | O_CREAT, 777);
        if(fd < 0){
            printf("无法写文件!!! %s\n", strerror(errno));
            exit(-1);
        }
        if(write(fd, restore_file_cache, file_cache_offset) != file_cache_offset){
            printf("Restore, write file error!!!\n");
            exit(-1);
        }
        close(fd);
    }

    gettimeofday(&t1, NULL);
    uint64_t single_dedup = (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec;
    printf("Backup duration:%lu us\n", single_dedup);
    
    return 0;
}