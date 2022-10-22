#include "fastcdc.h"
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

#define FILE_CACHE (1024*1024*1024)
#define CONTAINER_SIZE (4*1024*1024) //4MB容器

// <SHA1 20B, ContainerNumber 2B, offset 4B, size 2B, ContainerInnerIndex 2B>
#define META_DATA_SIZE 30
class Chunk{
    private:
        uint8_t fingerprint[SHA_DIGEST_LENGTH];
        uint16_t ContainerNumber;
        uint32_t ContainerInnerOffset;
        uint16_t size;
        uint16_t ContainerInnerIndex;
    public:
        Chunk(){}
        uint16_t getSize(){return size;}
        uint16_t getConNum(){return ContainerNumber;}
        uint32_t getInnerOffset(){return ContainerInnerOffset;}
        uint8_t* getFingerprint(){return fingerprint;}
        void initWithCompact(char*);
        void initWithValue(char*,uint16_t,uint32_t,uint16_t,uint16_t);
        std::string getDataFromDisk();
        void saveToDisk();
        static char restore_container_buf[CONTAINER_SIZE];
        static Chunk* lookupChunkMeta(const unsigned char*);
};
char Chunk::restore_container_buf[CONTAINER_SIZE] = {0};

char* fingerprintsFilePath = "./fingerprints.meta";
char* fileRecipesPath = "./FileRecipes";
char* containersPath = "./Containers";

enum TASK_TYPE{
    TASK_RESTORE,
    TASK_WRITE,
    NOT_CHOOSED
};

struct option long_options[] = {
    { "task", required_argument, NULL, 't' },
    { "InputFile", required_argument, NULL, 'i' },
    { "RestorePath", required_argument, NULL, 'r' },
    { "RestoreVersion", required_argument, NULL, 'v' },
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

Chunk* Chunk::lookupChunkMeta(const unsigned char* new_hash){
    int fd = open(fingerprintsFilePath, O_RDONLY);
    if (fd < 0) {
        std::cout << "lookupChunkMeta open error!\n";
        exit(-1);
    }

    char meta_buf[META_DATA_SIZE];
    Chunk* ans = new Chunk();
    for(int i=0; ;i++){
        int n_read = read(fd, meta_buf, META_DATA_SIZE);
        if(n_read == 0) 
            return nullptr;
        if(n_read < 0){
            std::cout<< "lookupChunkMeta read error!"<<std::endl;
            exit(-1);
        } 
        if(strncmp(meta_buf, (char*)(new_hash), SHA_DIGEST_LENGTH) == 0){
            ans->initWithCompact(meta_buf);
            return ans;
        }
    } 
    close(fd);
    return nullptr;
}

void Chunk::initWithCompact(char* meta){
    memcpy(this->fingerprint, meta, SHA_DIGEST_LENGTH);
    memcpy(&this->ContainerNumber, meta+20, 2);
    memcpy(&this->ContainerInnerOffset, meta+22, 4);
    memcpy(&this->size, meta+26, 2);
    memcpy(&this->ContainerInnerIndex, meta+28, 2);
}

void Chunk::initWithValue(char* hash, uint16_t cn, uint32_t off, uint16_t size, uint16_t innerIndex){
    memcpy(this->fingerprint, hash, SHA_DIGEST_LENGTH);
    this->ContainerNumber = cn;
    this->ContainerInnerOffset = off;
    this->size = size;
    this->ContainerInnerIndex = innerIndex;
}

void Chunk::saveToDisk(){
    int fd = open(fingerprintsFilePath, O_WRONLY | O_APPEND);
    if(fd < 0){
        printf("saveToDisk open error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }

    char meta_buf[META_DATA_SIZE]={0};
    memcpy(meta_buf, this->fingerprint,  SHA_DIGEST_LENGTH);
    memcpy(meta_buf+SHA_DIGEST_LENGTH, &this->ContainerNumber,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2, &this->ContainerInnerOffset,  4);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2+4, &this->size,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2+4+2, &this->ContainerInnerIndex,  2);

    if(write(fd, meta_buf, META_DATA_SIZE) != META_DATA_SIZE){
        printf("saveToDisk write error, id %d, %s\n", errno, strerror(errno));
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
    return std::string(Chunk::restore_container_buf + this->ContainerInnerOffset, this->size);
}

unsigned int getFileSize(char* file_path){
    int fd = open(file_path, O_RDONLY);
    if(fd < 0){
        printf("getFileSize open error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
    struct stat _stat;
    if(fstat(fd, &_stat) < 0){
        printf("getFileSize fstat error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
    return _stat.st_size / (SHA_DIGEST_LENGTH); 
}

unsigned int getFilesNum(char* dirPath){
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
    int fd = open(recipe_name.data(), O_RDWR | O_CREAT);
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
    int fd = open(recipe_name.data(), O_RDONLY);
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
    int fd = open(container_name.data(), O_RDWR | O_CREAT);
    if(write(fd, container_buf, len) != len){
        printf("saveContainer write error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
}

uint16_t cdc_origin_64(unsigned char *p, uint32_t n) {
    uint64_t fingerprint = 0, digest;
    uint16_t i = MinSize;
    if (n <= MinSize)  
        return n;
    if (n > MaxSize) 
        n = MaxSize;

    for ( ;i<n ;i++) {
        fingerprint = (fingerprint << 1) + (GEARv2[p[i]]);
        if ((!(fingerprint & FING_GEAR_08KB_64)))
            return i;
    }
    return n;
}

void fastCDC_init(void) {
    unsigned char md5_digest[16];
    uint8_t seed[SeedLength];
    for (int i = 0; i < SymbolCount; i++) {
        for (int j = 0; j < SeedLength; j++) {
            seed[j] = i;
        }
        g_global_matrix[i] = 0;
        MD5(seed, SeedLength, md5_digest);
        memcpy(&(g_global_matrix[i]), md5_digest, 4);
        g_global_matrix_left[i] = g_global_matrix[i] << 1;
    }

    // 64 bit init
    for (int i = 0; i < SymbolCount; i++) {
        LEARv2[i] = GEARv2[i] << 1;
    }

    MinSize = 8192 / 4;
    MaxSize = 8192 * 4;    // 32768;
    Mask_15 = 0xf9070353;  //  15个1
    Mask_11 = 0xd9000353;  //  11个1
    Mask_11_64 = 0x0000d90003530000;
    Mask_15_64 = 0x0000f90703530000;
    MinSize_divide_by_2 = MinSize / 2;
}

void printBytes(char* s, int len){
    for(int i=0; i<=len-1; i++)
        printf("%x", s[i]);
    printf("\n");
}

int main(int argc, char** argv){
    enum TASK_TYPE task_type = NOT_CHOOSED;
    char* input_file_path = nullptr;
    char* restore_file_path = nullptr;
    uint8_t restore_version = -1;

    int c;
    int option_index = 0;
    while ((c = getopt_long(argc, argv, "t:i:r:v:", long_options, &option_index)) != -1) {
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
            default:
                printf("Not support option: %c\n", c);
                exit(-1);
                break;
        }
    }

    if(task_type == TASK_WRITE){
        int current_version = getFilesNum(fileRecipesPath);

        int fd = open(input_file_path, O_RDONLY);
        if(fd < 0){
            printf("open file error, id %d, %s\n", errno, strerror(errno));
            exit(-1);
        }

        int dedup_chunks = 0;
        int sum_chunks = 0;
        unsigned char* file_cache = (unsigned char*)malloc(FILE_CACHE);
        unsigned char SHA_buf[SHA_DIGEST_LENGTH]={0};
        unsigned char meta_data_buf[META_DATA_SIZE]={0};
        std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹

        uint16_t container_index = getFilesNum(containersPath);
        uint32_t container_inner_offset = 0;
        uint16_t container_inner_index = 0;
        unsigned char container_buf[CONTAINER_SIZE]={0};
        unsigned int container_buf_pointer = 0;

        uint32_t n_read = read(fd, file_cache, FILE_CACHE);
        uint32_t file_offset = 0;
        uint16_t chunk_length = 0;
        int sum_chunk_length = 0;
        int dedup_size = 0;
        int hash_collision_sum = 0;

        fastCDC_init();
        if(n_read < 0){
            printf("read file error, id %d, %s\n", errno, strerror(errno));
            exit(-1);
        } 
        while(1){
            if(file_offset >= n_read)
                break;
            
            chunk_length = cdc_origin_64(file_cache + file_offset, n_read - file_offset);
            memset(SHA_buf, 0, SHA_DIGEST_LENGTH);
            SHA1(file_cache + file_offset, chunk_length, SHA_buf);
            sum_chunks ++;
            sum_chunk_length += chunk_length;

            Chunk* search_chunk = Chunk::lookupChunkMeta(SHA_buf);
            printf("%d %d\n", container_index, container_inner_offset);
            if(search_chunk == nullptr){
                // insert meta data
                Chunk new_chunk;
                new_chunk.initWithValue((char*)SHA_buf, container_index, container_inner_offset, chunk_length, container_inner_index);
                new_chunk.saveToDisk();
                
                // file recipe
                file_recipe.push_back(std::string((char*)SHA_buf, SHA_DIGEST_LENGTH));

                // Container
                if(container_buf_pointer + chunk_length >= CONTAINER_SIZE){
                    // flush
                    saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
                    memset(container_buf, 0, CONTAINER_SIZE);
                    container_index++;
                    container_inner_offset = 0;
                    container_buf_pointer = 0;
                    container_inner_index = 0;
                }
                memcpy(container_buf + container_buf_pointer, 
                        file_cache + file_offset, chunk_length);
                container_inner_offset += chunk_length;
                container_buf_pointer += chunk_length;
                container_inner_index ++;
                delete search_chunk;
            }else{  
                // 遇到问题，块的长度不一样，但hash一样
                
                dedup_chunks ++;
                dedup_size += chunk_length;
                file_recipe.push_back(std::string((char*)SHA_buf, SHA_DIGEST_LENGTH));
                delete search_chunk;
            }
            file_offset += chunk_length;
        }
        // 最后一个container
        if(container_buf_pointer > 0)
            saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
        // 唯一的一个file recipe
        saveFileRecipe(file_recipe, fileRecipesPath);
        // 重删统计
        double dedup_ratio = double(dedup_chunks) / sum_chunks;
        printf("-- Dedup statistics -- \n");
        printf("Dedup chunks %d\n", dedup_chunks);
        printf("Sum chunks %d\n", sum_chunks);
        printf("Sum chunk length %d\n", sum_chunk_length);
        printf("Dedup size %d\n", dedup_size);
        printf("Dedup ratio %.2f%\n", dedup_ratio*100);

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
        printf("file_recipe size = %d\n", file_recipe.size());
        printf("file_cache_offset = %d\n", file_cache_offset);
        //最后写文件
        int fd = open(restore_file_path, O_RDWR | O_CREAT);
        if(fd < 0){
            printf("无法写文件!!! %s\n", strerror(errno));
            exit(-1);
        }
        if(write(fd, restore_file_cache, file_cache_offset) != file_cache_offset){
            printf("Restore, write file error!!!\n");
            exit(-1);
        }
    }
    return 0;
}