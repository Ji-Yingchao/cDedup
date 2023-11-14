#include "lz4.h"

class Compressor{
private:
    long long chunk_num;
    long long size_before_compression;
    long long size_after_compression;

public:
    Compressor(){
        chunk_num = 0;
        size_before_compression = 0;
        size_after_compression = 0;
    }
    void compress(unsigned char*, long long);

    // getter
    long long get_chunk_num(){return this->chunk_num;}
    long long get_size_before_compression(){return this->size_before_compression;}
    long long get_size_after_compression(){return this->size_after_compression;}
};