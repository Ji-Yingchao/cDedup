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
        double total_time1 = 0.0, total_time2 = 0.0, total_time3 = 0.0;
    
    private:
        uint64_t container_read_count;
};
#endif