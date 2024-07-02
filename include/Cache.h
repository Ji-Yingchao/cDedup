#ifndef CACHE_H
#define CACHE_H
#include <iostream>
#include"MetadataManager.h"
class Cache{
    public:
        virtual std::string getChunkData(ENTRY_VALUE ev)=0;
        uint64_t getContainerReadCount(){return this->container_read_count;};
        void addContainerReadCount(){this->container_read_count++;};
        void initContainerReadCount(){this->container_read_count = 0;};
    
    private:
        uint64_t container_read_count;
};
#endif