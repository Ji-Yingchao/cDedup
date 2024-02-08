#include "MetadataManager.h"
#include "assert.h" 
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

MetadataManager *GlobalMetadataManagerPtr;

void MetadataManager::load(){
    int metadata_size = sizeof(SHA1FP) + sizeof(ENTRY_VALUE);
    printf("-----------------------Loading FP-index-----------------------\n");
    printf("Loading index..\n");

    unsigned char* metadata_cache = (unsigned char*)malloc(FILE_CACHE);
    int fd = open(this->metadata_file_path.c_str(), O_RDONLY);
    if(fd < 0)
        printf("MetadataManager::load error\n");
    int n = read(fd, metadata_cache, FILE_CACHE);
    int entry_count = n/metadata_size;
    SHA1FP tmp_fp;
    ENTRY_VALUE tmp_value;

    for(int i=0; i<=entry_count-1; i++){
        memcpy(&tmp_fp, metadata_cache+i*metadata_size, sizeof(SHA1FP));
        memcpy(&tmp_value, metadata_cache+i*metadata_size+sizeof(SHA1FP), sizeof(ENTRY_VALUE));

        this->fp_table_origin.emplace(tmp_fp, tmp_value);
    }

    close(fd);
    free(metadata_cache);
    printf("metadata table load %d items\n", entry_count);
}

void MetadataManager::save(){
    printf("-----------------------Saving FP-index-----------------------\n");
    printf("Saving index..\n");

    // Appending new fingerprints and entry value to FP-index;
    int fd = open(this->metadata_file_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0777);
    if(fd < 0){
        perror("Saving fp index error, the reason is ");
        exit(-1);
    }

    int count = 0;
    for(auto item : this->fp_table_added){
        write(fd, &item.first, sizeof(SHA1FP));
        write(fd, &item.second, sizeof(ENTRY_VALUE));
        count++;
    }
    printf("New added item %d\n", count);

    close(fd);
}

LookupResult MetadataManager::dedupLookup(SHA1FP *sha1){
    auto dedupIter = this->fp_table_origin.find(*sha1);
    if(dedupIter != this->fp_table_origin.end()){
        return Dedup;
    }

    dedupIter = this->fp_table_added.find(*sha1);
    if(dedupIter != this->fp_table_added.end()){
        return Dedup;
    }

    return Unique;
}

void MetadataManager::addNewEntry(const SHA1FP *sha1, const ENTRY_VALUE *value){
    this->fp_table_added.emplace(*sha1, *value);
}

ENTRY_VALUE MetadataManager::getEntry(const SHA1FP *sha1){
    return this->fp_table_origin[*sha1];
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