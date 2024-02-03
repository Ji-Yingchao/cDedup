#include "MetadataManager.h"
#include "assert.h" 
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

MetadataManager *GlobalMetadataManagerPtr;

int MetadataManager::load(){
    printf("-----------------------Loading FP-index-----------------------\n");
    printf("Loading index..\n");

    unsigned char* metadata_cache = (unsigned char*)malloc(FILE_CACHE);
    int fd = open(this->metadata_file_path.c_str(), O_RDONLY);
    if(fd < 0)
        printf("MetadataManager::load error\n");
    int n = read(fd, metadata_cache, FILE_CACHE);
    int entry_count = n/META_DATA_SIZE;
    SHA1FP tmp_fp;
    ENTRY_VALUE tmp_value;

    for(int i=0; i<=entry_count-1; i++){
        memcpy(&tmp_fp, metadata_cache+i*META_DATA_SIZE, SHA1_LENGTH);
        memcpy(&tmp_value, metadata_cache+i*META_DATA_SIZE+SHA1_LENGTH, VALUE_LENGTH);

        this->fp_table_origin.emplace(tmp_fp, tmp_value);
    }

    close(fd);
    free(metadata_cache);
    printf("metadata table load %d items\n", entry_count);
}

int MetadataManager::save(){
    printf("-----------------------Saving FP-index-----------------------\n");
    printf("Saving index..\n");

    // if(0 == access(this->metadata_file_path.c_str(), F_OK))
    //     remove(this->metadata_file_path.c_str());

    int fd = open(this->metadata_file_path.c_str(), O_WRONLY | O_CREAT, 0777);
    if(fd < 0){
        perror("Saving fp index error, the reason is ");
        exit(-1);
    }
    lseek(fd, 0, SEEK_SET);
    ftruncate(fd,0);

    int count = 0;
    
    for(auto item : this->fp_table_added){
        write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
        write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        count++;
    }
    printf("New added item %d\n", count);

    for(auto item : this->fp_table_origin){
        write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
        write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        count++;
    }
    printf("total item %d\n", count);

    close(fd);
}

LookupResult MetadataManager::dedupLookup(SHA1FP sha1){
    auto dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_origin.end()){
        return Dedup;
    }

    dedupIter = this->fp_table_added.find(sha1);
    if(dedupIter != this->fp_table_added.end()){
        return Dedup;
    }

    return Unique;
}

int MetadataManager::addNewEntry(SHA1FP sha1, ENTRY_VALUE value){
    auto dedupIter = this->fp_table_added.find(sha1);
    assert(dedupIter == this->fp_table_added.end());
    this->fp_table_added.emplace(sha1, value);
    return 0;
}

int MetadataManager::addRefCnt(const SHA1FP sha1){
    auto dedupIter = this->fp_table_added.find(sha1);
    if(dedupIter != this->fp_table_added.end())
        return ++dedupIter->second.ref_cnt;
    dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_added.end())
        return ++dedupIter->second.ref_cnt;
    printf("addRefCnt: did not find\n");
}

int MetadataManager::decRefCnt(const SHA1FP sha1){
    auto dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_origin.end()){
        if(dedupIter->second.ref_cnt > 1)
            return --dedupIter->second.ref_cnt;
        else{
            fp_table_origin.erase(dedupIter);
            return 0;
        }
    }
    printf("decRefCnt: did not find\n");
}

ENTRY_VALUE MetadataManager::getEntry(const SHA1FP sha1){
    return this->fp_table_origin[sha1];
}


int MetadataManager::chunkOffsetDec(SHA1FP sha1, int oft, int len){
    auto dedupIter = this->fp_table_origin.find(sha1);
    if(dedupIter != this->fp_table_origin.end()){
        if(dedupIter->second.offset > oft)
            dedupIter->second.offset -= len;
        return 0;
    }

    dedupIter = this->fp_table_added.find(sha1);
    if(dedupIter != this->fp_table_added.end()){
        if(dedupIter->second.offset > oft)
            dedupIter->second.offset -= len;
        return 0;
    }
    printf("MetadataManager::chunkOffsetDec error\n");
    exit(-1);
}