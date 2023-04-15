#include "MetadataManager.h"
#include "assert.h" 
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int MetadataManager::load(){
    printf("-----------------------Loading index-----------------------\n");
    printf("Loading index..\n");

    unsigned char* metadata_cache = (unsigned char*)malloc(FILE_CACHE);
    int fd = open(this->metadata_file_path.c_str(), O_RDONLY);
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
    printf("-----------------------Saving index-----------------------\n");
    printf("Saving index..\n");

    int fd = open(this->metadata_file_path.c_str(), O_APPEND | O_RDWR);
    int count = 0;
    for(auto item : this->fp_table_added){
        write(fd, (uint8_t*)&item.first, sizeof(SHA1FP));
        write(fd, (uint8_t*)&item.second, sizeof(ENTRY_VALUE));
        count++;
    }

    printf("New added item %d\n", count);
    close(fd);
}

LookupResult MetadataManager::dedupLookup(const SHA1FP &sha1){
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

int MetadataManager::addNewEntry(const SHA1FP sha1, const ENTRY_VALUE value){
    auto dedupIter = this->fp_table_added.find(sha1);
    assert(dedupIter == this->fp_table_added.end());
    this->fp_table_added.emplace(sha1, value);
    return 0;
}

ENTRY_VALUE MetadataManager::getEntry(const SHA1FP sha1){
    return this->fp_table_origin[sha1];
}