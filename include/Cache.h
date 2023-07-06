#ifndef CACHE_H
#define CACHE_H
#include <iostream>
#include"MetadataManager.h"
class Cache{
    public:
        virtual std::string getChunkData(ENTRY_VALUE ev)=0;
};
#endif