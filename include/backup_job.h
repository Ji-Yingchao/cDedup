// backup_job.h
#ifndef BACKUP_JOB_H
#define BACKUP_JOB_H

#include <cstdint>

// 结构体定义
struct backup_job{
    uint64_t hash_collision_sum;
    uint64_t sum_chunks;
    uint64_t sum_size;
    uint64_t dedup_chunks;
    uint64_t dedup_size;
    uint64_t file_num;
};

// 声明全局变量（定义放在 backup_job.cpp）
extern backup_job bj;

void print_backup_job(const backup_job& job, float throughput);

#endif // BACKUP_JOB_H
