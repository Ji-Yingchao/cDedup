#include "IndependentCache.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

std::string IndependentCache::getChunkData(ENTRY_VALUE ev){
    // find delta cache
    if(ev.container_number > base_container_max_value){
        if(ev.container_number != delta_cache.first){
            this->loadDeltaContainer(ev.container_number);
        }
        return std::string(delta_cache.second, ev.offset, ev.chunk_length);
    }

    // find base cache
    auto numberIter = this->base_cache.find(ev.container_number);
    if(numberIter != this->base_cache.end()){ //cache hit
        LRUstack.erase(numberIter->second.second);
        LRUtimestamp++;
        LRUstack[LRUtimestamp] = ev.container_number;
        numberIter->second.second = LRUtimestamp;
        
        return std::string(base_cache[ev.container_number].first, ev.offset, ev.chunk_length);
    }else{ //cache miss
        
        if(base_cache.size() >= this->base_cache_max_size){
            evictContainerLRU();
        }
        this->loadBaseContainer(ev.container_number);
        LRUtimestamp++;
        LRUstack[LRUtimestamp] = ev.container_number;
        base_cache[ev.container_number].second = LRUtimestamp;

        return std::string(base_cache[ev.container_number].first, ev.offset, ev.chunk_length);
    }
}

void IndependentCache::loadDeltaContainer(int container_index){
    std::string container_name(this->containers_path);
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDONLY | O_DIRECT);

    memset(this->container_buf, 0, CONTAINER_SIZE);
    int n = read(fd, this->container_buf, CONTAINER_SIZE);

    std::string content(this->container_buf , n);

    this->delta_cache.first = container_index;
    this->delta_cache.second = content;

    // 数容器数量
    //this->addReadContainer(container_index, this->read_delta_containers);
    this->read_delta_containers.push_back(container_index);

    close(fd);
}

void IndependentCache::loadBaseContainer(int container_index){
    std::string container_name(this->containers_path);
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDONLY | O_DIRECT);

    memset(this->container_buf, 0, CONTAINER_SIZE);
    int n = read(fd, this->container_buf, CONTAINER_SIZE); // 可能塞不满

    std::string content(this->container_buf , n);

    this->base_cache[container_index].first = content;

    // 数容器数量
    //this->addReadContainer(container_index, this->read_base_containers);
    this->read_base_containers.push_back(container_index);

    close(fd);
}

void IndependentCache::evictContainerLRU(){
    base_cache.erase(LRUstack.begin()->second);
    LRUstack.erase(LRUstack.begin());
}


// container 
int IndependentCache::getReferenceContainerCount(){
    return this->read_base_containers.size() + this->read_delta_containers.size();
};

void IndependentCache::removeDuplicates(std::vector<int>& containers) {
    std::unordered_set<int> unique_elements(containers.begin(), containers.end());
    containers.assign(unique_elements.begin(), unique_elements.end());
}

void IndependentCache::printContainers(int base_container_max_value){
    // 读取容器的次数
    int container_read_count = this->getReferenceContainerCount();
    printf("Read Container Count: %d\n", container_read_count);
    printf("Read Base Container Count: %ld\n", this->read_base_containers.size());
    printf("Read Delta Container Count: %ld\n", this->read_delta_containers.size());

    // 去重后是引用容器的个数
    this->removeDuplicates(this->read_base_containers);
    this->removeDuplicates(this->read_delta_containers);
    int reference_containers_count = this->getReferenceContainerCount(); 
    printf("Reference Container Count: %d\n", reference_containers_count);
    printf("Reference Base Container Count: %ld\n", this->read_base_containers.size());
    printf("Reference Delta Container Count: %ld\n", this->read_delta_containers.size());
}