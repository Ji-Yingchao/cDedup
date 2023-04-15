#include"ContainerCache.h"

std::string ContainerCache::getChunkData(ENTRY_VALUE ev){
    auto numberIter = this->container_number_set.find(ev.container_number);
    if(numberIter != this->container_number_set.end()){
        //cache hit
    }else{
        // cache miss
    }
}