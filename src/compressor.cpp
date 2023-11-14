#include "compressor.h"
#include "unistd.h"
#include "stdlib.h"

void Compressor::compress(unsigned char* buff, long long len){
    char *dest = (char*)malloc(sizeof(char)*len);
    long long compressed_size = LZ4_compress_default((char*)buff, dest, len, len);

    this->chunk_num ++;
    this->size_before_compression += len;
    this->size_after_compression += compressed_size;

    free(dest);
    return ;
}