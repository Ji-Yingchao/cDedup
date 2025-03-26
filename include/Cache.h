#ifndef CACHE_H
#define CACHE_H
#include <iostream>
#include <unordered_set>
#include <algorithm>
#include"MetadataManager.h"
class Cache{
    public:
        virtual std::string getChunkData(ENTRY_VALUE ev)=0;

        uint64_t getContainerReadCount(){return this->container_read_count;};
        void addContainerReadCount(){this->container_read_count++;};
        void initContainerReadCount(){this->container_read_count = 0;};
        double total_time1 = 0.0, total_time2 = 0.0, total_time3 = 0.0;

        void removeDuplicates() {
            std::unordered_set<uint64_t> unique_elements(this->reference_containers.begin(), this->reference_containers.end());
            this->reference_containers.assign(unique_elements.begin(), unique_elements.end());
        }
        uint64_t getReferenceContainerCount(){return this->reference_containers.size();};
        void addReferenceContainer(int num) {
            this->reference_containers.push_back(num);
        }
    
    private:
        uint64_t container_read_count;
        std::vector<uint64_t> reference_containers;
};
#endif