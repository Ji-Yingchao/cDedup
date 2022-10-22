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
        void init(char*);
        std::string getDataFromDisk();
        void saveToDisk();
        static Chunk* lookupChunkMeta(unsigned char*);
};

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

Chunk* Chunk::lookupChunkMeta(unsigned char* new_hash){
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
            ans->init(meta_buf);
            return ans;
        }
    } 
    return nullptr;
}

void Chunk::init(char* meta){
    memcpy(&this->fingerprint, meta, SHA_DIGEST_LENGTH);
    memcpy(&this->ContainerNumber, meta+20, 2);
    memcpy(&this->ContainerInnerOffset, meta+22, 4);
    memcpy(&this->size, meta+26, 2);
    memcpy(&this->ContainerInnerIndex, meta+28, 2);
}

void Chunk::saveToDisk(){
    int fd = open(fingerprintsFilePath, O_WRONLY | O_APPEND);
    if(fd < 0){
        printf("saveToDisk open error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }

    char meta_buf[META_DATA_SIZE]={0};
    memcpy(meta_buf, &this->fingerprint,  SHA_DIGEST_LENGTH);
    memcpy(meta_buf+SHA_DIGEST_LENGTH, &this->ContainerNumber,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2, &this->ContainerInnerOffset,  4);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2+4, &this->size,  2);
    memcpy(meta_buf+SHA_DIGEST_LENGTH+2+4+2, &this->ContainerInnerIndex,  2);

    if(write(fd, meta_buf, META_DATA_SIZE) != META_DATA_SIZE){
        printf("saveToDisk write error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
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
    for(auto x : file_recipe)
        write(fd, x.data(), strlen(x.data()));
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

int cdc_origin_64(unsigned char *p, int n) {
    uint64_t fingerprint = 0, digest;
    int i = MinSize;
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

        int container_index = getFilesNum(containersPath);
        int container_inner_offset = 0;
        int container_inner_index = 0;
        unsigned char container_buf[CONTAINER_SIZE]={0};
        unsigned int container_buf_pointer = 0;

        int n_read = read(fd, file_cache, FILE_CACHE);
        int file_offset = 0;
        int chunk_length = 0;
        fastCDC_init();
        if(n_read < 0){
            printf("read file error, id %d, %s\n", errno, strerror(errno));
            exit(-1);
        } 
        while(1){
            if(file_offset >= n_read){
                printf("---chunk file over---\n");
                break;
            }

            chunk_length = cdc_origin_64(file_cache + file_offset, n_read - file_offset);
            SHA1(file_cache + file_offset, chunk_length, SHA_buf);
            sum_chunks ++;
            
            Chunk* search_chunk = Chunk::lookupChunkMeta(SHA_buf);
            if(search_chunk == nullptr){
                // insert meta data
                // <fingerpirnt containerIndex containerInnerOffset chunkSize containerInnerIndex>
                

                // file recipe
                file_recipe.push_back(std::string((char*)SHA_buf, SHA_DIGEST_LENGTH));

                // Container
                if(container_buf_pointer + chunk_length >= CONTAINER_SIZE){
                    // flush
                    saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
                    container_buf_pointer = 0;
                    memset(container_buf, 0, CONTAINER_SIZE);
                    container_index++;
                    container_inner_offset = 0;
                }else{
                    memcpy(container_buf + container_buf_pointer, 
                            file_cache + file_offset, chunk_length);
                    container_inner_offset++;
                    container_buf_pointer += chunk_length;
                }
            }else{
                dedup_chunks++;
                file_recipe.push_back(std::string((char*)SHA_buf, SHA_DIGEST_LENGTH));
            }
            file_offset += chunk_length;


        }
        // 最后一个container
        if(container_buf_pointer > 0)
            saveContainer(container_index, container_buf, container_buf_pointer, containersPath);
        // 唯一的一个file recipe
        saveFileRecipe(file_recipe, fileRecipesPath);
        // 重删统计
        double dedup_ratio = dedup_chunks / sum_chunks;
        printf("-- Dedup statistics -- \n");
        printf("Dedup chunks %d\n", dedup_chunks);
        printf("Sum chunks %d\n", sum_chunks);
        printf("Dedup ratio %.2f%\n", dedup_ratio*100);
    }else if(task_type == TASK_RESTORE){

    }
    return 0;
}