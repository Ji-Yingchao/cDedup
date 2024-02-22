#ifndef __GLOBALSTAT_H__
#define __GLOBALSTAT_H__
#include<iostream>
#include <string.h>
#include"cJSON.h"
#include <unistd.h>
using namespace std;


// 每次重删任务前请手动将json item置0
class GlobalStat{
public:
    static GlobalStat& getInstance() {
        static GlobalStat instance;
        return instance;
    }

    // getters
    long long getLogicalSize(){return this->logical_size;}
    long long getPhysicalSize(){return this->physical_size;}
    double getDR(){return this->DR;}
    int getIndexSize(){return this->index_size;}
    int getFilesNum(){return this->files_num;}
    int getContainersNum(){return this->containers_num;}

    void parse_arguments(char * json_path)
    {
        char source[2000 + 1];
        FILE *fp = fopen(json_path, "r");
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
            char *item_name = param->string;

            int val_int = param->valueint;
            double val_dol = param->valuedouble;
            string val_str;
            if(param->valuestring)
                val_str = param->valuestring;

            //1. User indicate configurations
            if (strcmp(item_name, "LogicalSize") == 0) {
                GlobalStat::getInstance().setLogicalSize(atoll(val_str.c_str()));
            }else if (strcmp(item_name, "PhysicalSize") == 0) {
                GlobalStat::getInstance().setPhysicalSize(atoll(val_str.c_str()));
            }else if (strcmp(item_name, "DedupRatio") == 0) {
                GlobalStat::getInstance().setDR(val_dol);
            }else if (strcmp(item_name, "IndexSize") == 0) {
                GlobalStat::getInstance().setIndexSize(val_int);
            }else if (strcmp(item_name, "FilesNum") == 0) {
                GlobalStat::getInstance().setFilesNum(val_int);
            }else if (strcmp(item_name, "ContainersNum") == 0) {
                GlobalStat::getInstance().setContainersNum(val_int);
            }
        }

        //show
        printf("-------Parsing Old Global Arguments-------\n");
        printf("Init Logical Size %lld\n", GlobalStat::getInstance().getLogicalSize());
        printf("Init Physical Size %lld\n", GlobalStat::getInstance().getPhysicalSize());
        printf("Init DR %.2f\n", GlobalStat::getInstance().getDR());
        printf("Init IndexSize %d\n", GlobalStat::getInstance().getIndexSize());
        printf("Init FilesNum %d\n", GlobalStat::getInstance().getFilesNum());
        printf("Init ContainersNum %d\n", GlobalStat::getInstance().getContainersNum());
    }

    void save_arguments(char * json_path)
    {
        //Parsing路径和Saving路径务必一致
        char source[2000 + 1]={0};
        FILE *fp = fopen(json_path, "r+");
        if (fp != NULL) {
            size_t newLen = fread(source, sizeof(char), 1001, fp);
            if ( ferror( fp ) != 0 ) {
                fputs("Error reading file", stderr);
            } else {
                source[newLen++] = '\0'; /* Just to be safe. */
            }
        }else{
            perror("open json file failed");
        }

        cJSON *config = cJSON_Parse(source);
        
        cJSON_ReplaceItemInObject(config, "LogicalSize", cJSON_CreateString(to_string(GlobalStat::getInstance().getLogicalSize()).c_str()));
        cJSON_ReplaceItemInObject(config, "PhysicalSize", cJSON_CreateString(to_string(GlobalStat::getInstance().getPhysicalSize()).c_str()));
        cJSON_ReplaceItemInObject(config, "DedupRatio", cJSON_CreateNumber(GlobalStat::getInstance().getDR()));
        cJSON_ReplaceItemInObject(config, "IndexSize", cJSON_CreateNumber(GlobalStat::getInstance().getIndexSize()));
        cJSON_ReplaceItemInObject(config, "FilesNum", cJSON_CreateNumber(GlobalStat::getInstance().getFilesNum()));
        cJSON_ReplaceItemInObject(config, "ContainersNum", cJSON_CreateNumber(GlobalStat::getInstance().getContainersNum()));
        
        char *cjValue = cJSON_Print(config);
        ftruncate(fileno(fp), 0);
        fseek(fp, 0, SEEK_SET);
        int ret = fwrite((void*)cjValue, sizeof(char), strlen(cjValue), fp);
        if (ret < 0) {
            perror("write json file error!\n");
        }
        fclose(fp);
        free(cjValue);
        cJSON_Delete(config);

        //show
        printf("-------Saving New Global Arguments-------\n");
        printf("Logical Size %lld\n", GlobalStat::getInstance().getLogicalSize());
        printf("Physical Size %lld\n", GlobalStat::getInstance().getPhysicalSize());
        printf("Logical Size %lld KiB\n", GlobalStat::getInstance().getLogicalSize()/1024);
        printf("Physical Size %lld KiB\n", GlobalStat::getInstance().getPhysicalSize()/1024);
        printf("DR %.2f\n", GlobalStat::getInstance().getDR());
        printf("IndexSize %d\n", GlobalStat::getInstance().getIndexSize());
        printf("FilesNum %d\n", GlobalStat::getInstance().getFilesNum());
        printf("ContainersNum %d\n", GlobalStat::getInstance().getContainersNum());

    }

    void update_by_backup_job(long long backup_logical_size, long long backup_physical_size, 
                                long long chunk_num, long long unique_chunk_num, int containers_num){
        this->logical_size += backup_logical_size ;
        this->physical_size += backup_physical_size ;
        this->DR = ((double)this->logical_size - (double)this->physical_size)/ (double)this->logical_size;

        this->index_size += chunk_num * sizeof(SHA1FP); // file recipe
        this->index_size += unique_chunk_num*(sizeof(ENTRY_VALUE)+sizeof(SHA1FP)); // fp index
        this->files_num++; 
        this->containers_num += containers_num;
    }

private:
    // setters
    void setLogicalSize(long long n){this->logical_size = n;}
    void setPhysicalSize(long long n){this->physical_size = n;}
    void setDR(double n){this->DR = n;}
    void setIndexSize(int n){this->index_size = n;}
    void setFilesNum(int n){this->files_num = n;}
    void setContainersNum(int n){this->containers_num = n;}

    // 全局重删压缩信息
    long long logical_size;
    long long physical_size;
    double DR;
    int index_size;
    int files_num;
    int containers_num;
};

#endif