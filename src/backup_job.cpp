// backup_job.cpp
#include "backup_job.h"
#include <cstdio>
#include <cinttypes>

// 真正定义全局变量
backup_job bj;

void print_backup_job(const backup_job& job, float throughput){
    // printf("-----------------------Dedup statics----------------------\n");
    // printf("Hash collision num %" PRIu64 "\n",    bj.hash_collision_sum); // should be zero
    // printf("Sum chunks num % " PRIu64 "\n",       bj.sum_chunks);    //bj.sum_chunks统计当前版本数据，与sum_size不对应
    // printf("Sum data size %" PRIu64 "\n",         bj.sum_size);
    // printf("Average chunk size %" PRIu64 "\n",    bj.sum_size / bj.sum_chunks);
    // printf("Dedup chunks num %" PRIu64 "\n",      bj.dedup_chunks);    // bj.sum_size和bj.dedup_size统计所有数据
    // printf("Dedup data size %" PRIu64 "\n",       bj.dedup_size);
    printf("-----------------------statics----------------------\n");
    //printf("Throughput %.2f MiB/s\n",    throughput);   //throughput无意义，因为time是一个版本的，size是所有版本的
    printf("Actual DR %.4f \n", double(bj.sum_size) / double(bj.sum_size - bj.dedup_size) );
    //printf("Actual DR %.2f%\n",     double(bj.dedup_size) / double(bj.sum_size) *100);
}