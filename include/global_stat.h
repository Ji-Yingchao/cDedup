#ifndef __GLOBALSTAT_H__
#define __GLOBALSTAT_H__
#include<iostream>
#include <string.h>
#include"cJSON.h"
#include <unistd.h>
using namespace std;

// json好像不支持long long
// 每次重删任务前请手动将json item置0
class GlobalStat{
public:
    static GlobalStat& getInstance() {
        static GlobalStat instance;
        return instance;
    }

    // getters
    int getTotalSize(){return this->total_size_kb;}
    int getSizeBeforeCompression(){return this->size_before_compression_kb;}
    int getSizeAfterCompression(){return this->size_after_compression_kb;}
    double getDR(){return this->DR;}
    double getCR(){return this->CR;}
    double getDRR(){return this->DRR;}

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
            } else if (strcmp(item_name, "SizeBeforeCompression") == 0) {
                GlobalStat::getInstance().setSizeBeforeCompression(val_int);
            } else if (strcmp(item_name, "SizeAfterCompression") == 0) {
                GlobalStat::getInstance().setSizeAfterCompression(val_int);
            }else if (strcmp(item_name, "DeduplicationRatio") == 0) {
                GlobalStat::getInstance().setDR(val_dol);
            }else if (strcmp(item_name, "CompressionRatio") == 0) {
                GlobalStat::getInstance().setCR(val_dol);
            }else if (strcmp(item_name, "DataReductionRatio") == 0) {
                GlobalStat::getInstance().setDRR(val_dol);
            }
        }

        //show
        printf("-------Parsing Old Global Arguments-------\n");
        printf("Init Size %d\n", GlobalStat::getInstance().getTotalSize());
        printf("Init Size Before Compression %d\n", GlobalStat::getInstance().getSizeBeforeCompression());
        printf("Init Size After Compression %d\n", GlobalStat::getInstance().getSizeAfterCompression());
        printf("Init DR %.2f\n", GlobalStat::getInstance().getDR());
        printf("Init CR %.2f\n", GlobalStat::getInstance().getCR());
        printf("Init DRR %.2f\n", GlobalStat::getInstance().getDRR());
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
        cJSON_ReplaceItemInObject(config, "Size", cJSON_CreateNumber((double)GlobalStat::getInstance().getTotalSize()));
        cJSON_ReplaceItemInObject(config, "SizeBeforeCompression", cJSON_CreateNumber((double)GlobalStat::getInstance().getSizeBeforeCompression()));
        cJSON_ReplaceItemInObject(config, "SizeAfterCompression", cJSON_CreateNumber((double)GlobalStat::getInstance().getSizeAfterCompression()));
        cJSON_ReplaceItemInObject(config, "DeduplicationRatio", cJSON_CreateNumber(GlobalStat::getInstance().getDR()));
        cJSON_ReplaceItemInObject(config, "CompressionRatio", cJSON_CreateNumber(GlobalStat::getInstance().getCR()));
        cJSON_ReplaceItemInObject(config, "DataReductionRatio", cJSON_CreateNumber(GlobalStat::getInstance().getDRR()));
        
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
        printf("Size %d KiB\n", GlobalStat::getInstance().getTotalSize());
        printf("Size Before Compression %d KiB\n", GlobalStat::getInstance().getSizeBeforeCompression());
        printf("Size After Compression %d KiB\n", GlobalStat::getInstance().getSizeAfterCompression());
        printf("DR %.2f\n", GlobalStat::getInstance().getDR());
        printf("CR %.2f\n", GlobalStat::getInstance().getCR());
        printf("DRR %.2f\n", GlobalStat::getInstance().getDRR());
    }

    void update_kb(int backup_size_kb, 
                int backup_size_before_compression_kb,
                int backup_size_after_compression_kb){
        this->total_size_kb += backup_size_kb ;
        this->size_before_compression_kb += backup_size_before_compression_kb;
        this->size_after_compression_kb += backup_size_after_compression_kb;

        this->DR = 1 - (double)this->size_before_compression_kb / (double)this->total_size_kb;
        this->CR = 1 - (double)this->size_after_compression_kb / (double)this->size_before_compression_kb;
        this->DRR = 1 - (double)this->size_after_compression_kb / (double)this->total_size_kb;
    }

private:
    // setters
    void setTotalSize(int n){this->total_size_kb = n;}
    void setSizeBeforeCompression(int n){this->size_before_compression_kb = n;}
    void setSizeAfterCompression(int n){this->size_after_compression_kb = n;}
    void setDR(double n){this->DR = n;}
    void setCR(double n){this->CR = n;}
    void setDRR(double n){this->DRR = n;}

    // 全局重删压缩信息
    int total_size_kb;
    int size_before_compression_kb;
    int size_after_compression_kb;
    double DR;
    double CR;
    double DRR;
};

#endif