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
void fastCDC_init(int fastcdc_avg_size, int NC_level);

// origin fastcdc function
ssize_t FastCDC_without_NC(unsigned char *p, int n);

// normalized fastcdc
ssize_t FastCDC_with_NC(unsigned char *p, int n);

// FSC
ssize_t FSC_512(unsigned char *p, int n);
ssize_t FSC_4(unsigned char *p, int n);
ssize_t FSC_8(unsigned char *p, int n);
ssize_t FSC_16(unsigned char *p, int n);

#endif