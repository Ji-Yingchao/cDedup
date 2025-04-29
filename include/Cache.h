#ifndef CACHE_H
#define CACHE_H
#include <iostream>
#include <unordered_set>
#include <algorithm>
#include "MetadataManager.h"


class Cache{
    public:
        virtual std::string getChunkData(ENTRY_VALUE ev)=0;

        double total_time1 = 0.0, total_time2 = 0.0, total_time3 = 0.0;
};
#endif