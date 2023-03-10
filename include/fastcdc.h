/**
 * This is an implementation of fastCDC
 * The origin paper is Wen Xia, Yukun Zhou, Hong Jiang, Dan Feng, Yu Hua, Yuchong Hu, Yucheng Zhang, Qing Liu, "FastCDC: a Fast and Efficient Content-Defined Chunking Approach for Data Deduplication", in Proceedings of USENIX Annual Technical Conference (USENIX ATC'16), Denver, CO, USA, June 22–24, 2016, pages: 101-114
 * and Wen Xia, Xiangyu Zou, Yukun Zhou, Hong Jiang, Chuanyi Liu, Dan Feng, Yu Hua, Yuchong Hu, Yucheng Zhang, "The Design of Fast Content-Defined Chunking for Data Deduplication based Storage Systems", IEEE Transactions on Parallel and Distributed Systems (TPDS), 2020
 *
 */ 
#ifndef  FASTCDC_H
#define  FASTCDC_H

#include <openssl/md5.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <zlib.h>
#include "uthash.h"

// init function
void fastCDC_init(void);

// origin fastcdc function
int cdc_origin_64(unsigned char *p, int n);

// fastcdc with once rolling 2 bytes 
int rolling_data_2byes_64(unsigned char *p, int n);

// normalized fastcdc
int normalized_chunking_64(unsigned char *p, int n);

// normalized fastcdc with once rolling 2 bytes
int normalized_chunking_2byes_64(unsigned char *p, int n);

#endif