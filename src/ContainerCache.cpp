#include"ContainerCache.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

std::string ContainerCache::getChunkData(ENTRY_VALUE ev){
    auto numberIter = this->container_index_set.find(ev.container_number);
    if(numberIter != this->container_index_set.end()){
        //cache hit
        return std::string(cache[ev.container_number], ev.offset, ev.chunk_length);
    }else{
        //cache miss
        if(container_index_queue.size() >= this->cache_max_size){
            evictContainerFIFO();
        }
        this->loadContainer(ev.container_number);
        return std::string(cache[ev.container_number], ev.offset, ev.chunk_length);
    }
}

void ContainerCache::loadContainer(int container_index){
    struct timeval start1, end1,start2, end2;
    this->container_index_queue.push(container_index);
    this->container_index_set.insert(container_index);
    
    std::string container_name(this->containers_path);
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDONLY | O_DIRECT);

    memset(this->container_buf, 0, CONTAINER_SIZE);
    gettimeofday(&start2, NULL);
    int n = read(fd, this->container_buf, CONTAINER_SIZE); // 可能塞不满
    gettimeofday(&end2, NULL);
    int tmp = (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;
    this->total_time2 += (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;

    std::string content(this->container_buf , n);

    this->cache[container_index] = content;
    this->addContainerReadCount();
    close(fd);
}

void ContainerCache::evictContainerFIFO(){
    int container_index = this->container_index_queue.front();
    this->container_index_set.erase(container_index);
    this->container_index_queue.pop();

    cache.erase(container_index);
}