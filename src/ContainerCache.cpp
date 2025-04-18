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
    // 只数容器数量，不返回数据
    // if(numberIter != this->container_index_set.end()){
    //     //cache hit
    //     return std::string("aaa");
    // }else{
    //     //cache miss
    //     if(container_index_queue.size() >= this->cache_max_size){
    //         evictContainerFIFO();
    //     }
    //     this->loadContainer(ev.container_number);
    //     return std::string("bbb");
    // }
}

void ContainerCache::loadContainer(int container_index){
    //struct timeval start1, end1,start2, end2;
    this->container_index_queue.push(container_index);
    this->container_index_set.insert(container_index);
    
    //只数容器数量，所以注释
    std::string container_name(this->containers_path);
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDONLY | O_DIRECT);

    memset(this->container_buf, 0, CONTAINER_SIZE);
    //gettimeofday(&start2, NULL);
    int n = read(fd, this->container_buf, CONTAINER_SIZE); // 可能塞不满
    //gettimeofday(&end2, NULL);
    //int tmp = (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;
    //this->total_time2 += (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;

    std::string content(this->container_buf , n);

    this->cache[container_index] = content;

    // 数容器数量
    this->reference_containers.push_back(container_index);

    close(fd);
}

void ContainerCache::evictContainerFIFO(){
    int container_index = this->container_index_queue.front();
    this->container_index_set.erase(container_index);
    this->container_index_queue.pop();

    cache.erase(container_index);
}


// 统计恢复时的容器数量
int ContainerCache::getReferenceContainerCount(){
    return this->reference_containers.size();
};

// 去除重复的容器
void ContainerCache::removeDuplicates() {
    std::unordered_set<int> unique_elements(this->reference_containers.begin(), this->reference_containers.end());
    this->reference_containers.assign(unique_elements.begin(), unique_elements.end());
}

// 统计base容器和delta容器的个数
std::pair<size_t, size_t> ContainerCache::countBaseAndDelta(uint64_t threshold) {
    // 小于或等于 threshold 的容器是base
    size_t count_base = std::count_if(this->reference_containers.begin(), this->reference_containers.end(),
                                    [threshold](uint64_t value) { return value <= threshold; });
    size_t count_delta = std::count_if(this->reference_containers.begin(), this->reference_containers.end(),
                                        [threshold](uint64_t value) { return value > threshold; });
    return {count_base, count_delta};
}

void ContainerCache::printContainers(int base_container_max_value){
    // 读取容器的次数
    int container_read_count = this->getReferenceContainerCount();
    auto [base_counter, delta_container] = this->countBaseAndDelta(base_container_max_value);
    printf("Read Container Count: %d\n", container_read_count);
    printf("Read Base Container Count: %d\n", base_counter);
    printf("Read Delta Container Count: %d\n", delta_container);

    // 去重后是引用容器的个数
    this->removeDuplicates();
    int reference_containers_count = this->getReferenceContainerCount(); 
    auto [r_base_counter, r_delta_container] = this->countBaseAndDelta(base_container_max_value);
    printf("Reference Container Count: %d\n", reference_containers_count);
    printf("Reference Base Container Count: %d\n", r_base_counter);
    printf("Reference Delta Container Count: %d\n", r_delta_container);
}