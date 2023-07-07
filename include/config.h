#ifndef __CONFIG_H__
#define __CONFIG_H__
#include<iostream>
#include <string.h>
#include"cJSON.h"
using namespace std;

enum TASK_TYPE{
    TASK_RESTORE,
    TASK_WRITE,
    TASK_DELETE,
    NOT_CHOOSED
};

enum CHUNKING_METHOD{
    CDC,
    FSC,
    FULL_FILE
};

enum RESTORE_METHOD{
    NAIVE_RESTORE,  //无缓冲模式   
    CONTAINER_CACHE,//基于容器的缓冲
    CHUNK_CACHE,    //基于数据块的缓冲
    FAA_FIXED,      //一次性发送FAA
    FAA_ROLLING,    //FAA环形缓冲区
};

class Config{
    public:
        static Config& getInstance() {
          static Config instance;
          return instance;
        }

        // getters
        enum TASK_TYPE getTaskType(){return this->tt;}
        string getInputPath(){return this->input_path;}
        string getRestorePath(){return this->restore_path;}
        int getRestoreVersion(){return this->restore_version;}
        enum CHUNKING_METHOD getChunkingMethod(){return this->cm;}
        int getRestoreId(){return this->restore_id;}
        int getDeleteId(){return this->delete_id;}
        int getAvgChunkSize(){return this->avg_chunk_size;}
        int getNormalLevel(){return this->normal_level;}
        bool getMerkleTree(){return this->merkle_tree;}
        enum RESTORE_METHOD getRestoreMethod(){return this->rm;}

        string getFingerprintsFilePath(){return this->fingerprints_file_path;}
        string getFileRecipesPath(){return this->file_recipe_path;}        
        string getContainersPath(){return this->container_path;}     
        string getFullFileFingerprintsPath(){return this->full_file_fingerprints_path;}        
        string getFullFileStoragePath(){return this->full_file_storage_path;}

        string getMTL1(){return this->MTL1;}
        string getMTL2(){return this->MTL2;}
        string getMTL3(){return this->MTL3;}
        string getMTL4(){return this->MTL4;}
        string getMTL5(){return this->MTL5;}
        string getMTL6(){return this->MTL6;}

        // setters
        void setTask(char* s){this->tt = taskTypeTrans(s);}
        void setInputFile(char* s){this->input_path = s;}
        void setRestorePath(char* s){this->restore_path = s;}
        void setRestoreVersion(int n){this->restore_version = n;}
        void setChunkingMethod(char* s){this->cm = cmTypeTrans(s);}
        void setRestoreId(int n){this->restore_id = n;}
        void setDeleteId(int n){this->delete_id = n;}
        void setSize(int n){this->avg_chunk_size = n;}
        void setNormal(int n){this->normal_level = n;}
        void setMerkleTree(char* s){this->merkle_tree = yesNoTrans(s);}
        void setRestoreMethod(char* s){this->rm = restoreMethodTrans(s);}

        void setFingerprintsFilePath(char* s){this->fingerprints_file_path = s;}
        void setFileRecipesPath(char* s){this->file_recipe_path = s;}        
        void setContainersPath(char* s){this->container_path = s;}     
        void setFullFileFingerprintsPath(char* s){this->full_file_fingerprints_path = s;}        
        void setFullFileStoragePath(char* s){this->full_file_storage_path = s;}

        void setMTL1(char* s){this->MTL1 = s;}
        void setMTL2(char* s){this->MTL2 = s;}
        void setMTL3(char* s){this->MTL3 = s;}
        void setMTL4(char* s){this->MTL4 = s;}
        void setMTL5(char* s){this->MTL5 = s;}
        void setMTL6(char* s){this->MTL6 = s;}

        // you know
        void parse_argument(int argc, char **argv)
        {
            char source[2000 + 1];
            FILE *fp = fopen(argv[1], "r");
            if (fp != NULL) {
                size_t newLen = fread(source, sizeof(char), 1001, fp);
                if ( ferror( fp ) != 0 ) {
                    fputs("Error reading file", stderr);
                } else {
                    source[newLen++] = '\0'; /* Just to be safe. */
                }

                fclose(fp);
            }else{
                perror("open json file failed");
            }

            cJSON *config = cJSON_Parse(source);
            for (cJSON *param = config->child; param != nullptr; param = param->next) {
                char *name = param->string;
                char *valuestring = param->valuestring;
                int val_int = param->valueint;

                //1. User indicate configurations
                if (strcmp(name, "Task") == 0) {
                    Config::getInstance().setTask(valuestring);
                } else if (strcmp(name, "InputFile") == 0) {
                    Config::getInstance().setInputFile(valuestring);
                } else if (strcmp(name, "RestorePath") == 0) {
                    Config::getInstance().setRestorePath(valuestring);
                } else if (strcmp(name, "RestoreVersion") == 0) {
                    Config::getInstance().setRestoreVersion(val_int);
                } else if (strcmp(name, "ChunkingMethod") == 0) {
                    Config::getInstance().setChunkingMethod(valuestring);
                } else if (strcmp(name, "RestoreId") == 0) {
                    Config::getInstance().setRestoreId(val_int);
                } else if (strcmp(name, "DeleteId") == 0) {
                    Config::getInstance().setDeleteId(val_int);
                }else if (strcmp(name, "Size") == 0) {
                    Config::getInstance().setSize(val_int);
                }else if (strcmp(name, "Normal") == 0) {
                    Config::getInstance().setNormal(val_int);
                }else if (strcmp(name, "MerkleTree") == 0) {
                    Config::getInstance().setMerkleTree(valuestring);
                }else if (strcmp(name, "RestoreMethod") == 0) {
                    Config::getInstance().setRestoreMethod(valuestring);
                }
                //2. metadata configurations
                else if (strcmp(name, "fingerprintsFilePath") == 0) {
                    Config::getInstance().setFingerprintsFilePath(valuestring);
                } else if (strcmp(name, "fileRecipesPath") == 0) {
                    Config::getInstance().setFileRecipesPath(valuestring);
                } else if (strcmp(name, "containersPath") == 0) {
                    Config::getInstance().setContainersPath(valuestring);
                } else if (strcmp(name, "fullFileFingerprintsPath") == 0) {
                    Config::getInstance().setFullFileFingerprintsPath(valuestring);
                } else if (strcmp(name, "fullFileStoragePath") == 0) {
                    Config::getInstance().setFullFileStoragePath(valuestring);
                }
                else if (strcmp(name, "MTL1") == 0) {
                    Config::getInstance().setMTL1(valuestring);
                }else if (strcmp(name, "MTL2") == 0) {
                    Config::getInstance().setMTL2(valuestring);
                }else if (strcmp(name, "MTL3") == 0) {
                    Config::getInstance().setMTL3(valuestring);
                }else if (strcmp(name, "MTL4") == 0) {
                    Config::getInstance().setMTL4(valuestring);
                }else if (strcmp(name, "MTL5") == 0) {
                    Config::getInstance().setMTL5(valuestring);
                }else if (strcmp(name, "MTL6") == 0) {
                    Config::getInstance().setMTL6(valuestring);
                }
            }
        }

    private:
        // 读写任务参数
        enum TASK_TYPE tt;
        enum CHUNKING_METHOD cm;
        enum RESTORE_METHOD rm;
        string input_path;
        string restore_path;
        int restore_version;    // used for cdc
        int restore_id;         // used for full file dedup
        int delete_id;
        int avg_chunk_size;     // unit KiB
        int normal_level;
        bool merkle_tree;

        // 元数据相关参数
        string fingerprints_file_path; // 也可用作merkle tree L0
        string file_recipe_path;
        string container_path;
        string full_file_fingerprints_path;
        string full_file_storage_path;
        string MTL1, MTL2, MTL3, MTL4, MTL5, MTL6;

        Config() {
            avg_chunk_size = 4096;
            normal_level = 2;
            merkle_tree = false;
        }

        enum TASK_TYPE taskTypeTrans(char* s){
            if(strcmp(s, "write") == 0){
                return TASK_WRITE;
            }else if (strcmp(s, "restore") == 0){
                return TASK_RESTORE;
            }else if (strcmp(s, "delete") == 0){
                return TASK_DELETE;
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

        RESTORE_METHOD restoreMethodTrans(char* s){
            if(strcmp(s, "naive") == 0){
                return NAIVE_RESTORE;
            }else if (strcmp(s, "container") == 0){
                return CONTAINER_CACHE;
            }else if (strcmp(s, "chunk") == 0){
                return CHUNK_CACHE;
            }else if (strcmp(s, "faa_fixed") == 0){
                return FAA_FIXED;
            }else if (strcmp(s, "faa_rolling") == 0){
                return FAA_ROLLING;
            }else{
                printf("Not support restore method:%s\n", s);
                exit(-1);
            }
        }
};
#endif