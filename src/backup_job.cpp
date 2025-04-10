// backup_job.cpp
#include "backup_job.h"
#include <cstdio>
#include <cinttypes>

// 真正定义全局变量
backup_job bj;

void print_backup_job(const backup_job& job){
    printf("-----------------------Dedup statics----------------------\n");
    printf("Hash collision num %" PRIu64 "\n",    bj.hash_collision_sum); // should be zero
    printf("Sum chunks num % " PRIu64 "\n",       bj.sum_chunks);
    printf("Sum data size %" PRIu64 "\n",         bj.sum_size);
    printf("Average chunk size %" PRIu64 "\n",    bj.sum_size / bj.sum_chunks);
    printf("Dedup chunks num %" PRIu64 "\n",      bj.dedup_chunks);
    printf("Dedup data size %" PRIu64 "\n",       bj.dedup_size);
    //printf("-----------------------statics----------------------\n");
    //printf("Throughput %.2f MiB/s\n",    throughput);
    //printf("Dedup Ratio %.2f%\n",     double(bj.dedup_size) / double(bj.sum_size) *100);
}