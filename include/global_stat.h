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
    int getTotalSize(){return this->total_size_kb;}
    int getDedupSize(){return this->dedup_size_kb;}
    double getDR(){return this->DR;}

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

            //1. User indicate configurations
            if (strcmp(item_name, "Size") == 0) {
                GlobalStat::getInstance().setTotalSize(val_int);
            }else if (strcmp(item_name, "DeduplicationRatio") == 0) {
                GlobalStat::getInstance().setDR(val_dol);
            }
        }

        //show
        printf("-------Parsing Old Global Arguments-------\n");
        printf("Init Total Size %d\n", GlobalStat::getInstance().getTotalSize());
        printf("Init Dedup Size %d\n", GlobalStat::getInstance().getDedupSize());
        printf("Init DR %.2f\n", GlobalStat::getInstance().getDR());
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
        cJSON_ReplaceItemInObject(config, "Total Size", cJSON_CreateNumber((double)GlobalStat::getInstance().getTotalSize()));
        cJSON_ReplaceItemInObject(config, "Dedup Size", cJSON_CreateNumber((double)GlobalStat::getInstance().getDedupSize()));
        cJSON_ReplaceItemInObject(config, "DeduplicationRatio", cJSON_CreateNumber(GlobalStat::getInstance().getDR()));
        
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
        printf("Total Size %d KiB\n", GlobalStat::getInstance().getTotalSize());
        printf("Dedup Size %d KiB\n", GlobalStat::getInstance().getDedupSize());
        printf("DR %.2f\n", GlobalStat::getInstance().getDR());

    }

    void update_kb(int backup_total_size_kb, int backup_dedup_size_kb){
        this->total_size_kb += backup_total_size_kb ;
        this->dedup_size_kb += backup_dedup_size_kb ;
        this->DR = (double)this->dedup_size_kb / (double)this->total_size_kb;
    }

private:
    // setters
    void setTotalSize(int n){this->total_size_kb = n;}
    void setDedupSize(int n){this->dedup_size_kb = n;}
    void setDR(double n){this->DR = n;}

    // 全局重删压缩信息
    // json好像不支持long long，所以这里暂时用KiB
    int total_size_kb;
    int dedup_size_kb;
    double DR;

};

#endif