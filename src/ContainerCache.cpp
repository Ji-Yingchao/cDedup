#include "ContainerCache.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

string ContainerCache::getChunkData(ENTRY_VALUE ev){
    ContainerKey key = {ev.container_type, ev.container_number};

    //this->average_containers[ev.container_type]++;

    auto numberIter = this->container_index_set.find(key);
    if(numberIter != this->container_index_set.end()){
        //cache hit
        return string(cache[key], ev.offset, ev.chunk_length);
    }else{
        //cache miss
        if(container_index_queue.size() >= this->cache_max_size){
            evictContainerFIFO();
        }
        this->loadContainer(ev.container_number, ev.container_type);
        return string(cache[key], ev.offset, ev.chunk_length);
    }
}

void ContainerCache::loadContainer(int container_index, CONTAINER_TYPE container_type){
    //struct timeval start1, end1,start2, end2;
    ContainerKey key = {container_type, container_index};
    this->container_index_queue.push(key);
    this->container_index_set.insert(key);
    
    //只数容器数量，所以注释
    string container_path;
    if(container_type == HOT_CONTAINER) container_path = this->hot_containers_path;
    else container_path = this->containers_path;
    string container_name = container_path + "/" + container_type_to_string(container_type) + to_string(container_index);

    int fd = open(container_name.data(), O_RDONLY | O_DIRECT);
    if (fd == -1) {
        printf("open container error: %s\n", strerror(errno));
        printf("container path: %s\n", container_name.c_str());
        exit(-1);
    }

    memset(this->container_buf, 0, CONTAINER_SIZE);
    //gettimeofday(&start2, NULL);
    int n = read(fd, this->container_buf, CONTAINER_SIZE); // 可能塞不满
    //gettimeofday(&end2, NULL);
    //int tmp = (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;
    //this->total_time2 += (end2.tv_sec - start2.tv_sec) * 1000000 + end2.tv_usec - start2.tv_usec;

    string content(this->container_buf , n);

    this->cache[key] = content;

    // 数容器数量
    this->reference_containers[container_type].push_back(container_index);

    close(fd);
}

void ContainerCache::evictContainerFIFO(){
    ContainerKey container_index = this->container_index_queue.front();
    this->container_index_set.erase(container_index);
    this->container_index_queue.pop();

    cache.erase(container_index);
}


// 统计恢复时的容器数量
int ContainerCache::getReferenceContainerCount(){
    int total_count = 0;
    for (const auto& [type, ids] : this->reference_containers) {
        total_count += ids.size();

        //int x= this->average_containers[type]/ids.size();
        //printf("%s容器平均chunk数: %d\n", container_type_to_string(type).c_str(),x);
        //printf("%s: %d\n", container_type_to_string(type).c_str(),ids.size());
    }
    return total_count;
};

// 去除重复的容器
void ContainerCache::removeDuplicates() {
    for (auto& [type, ids] : this->reference_containers) {
        unordered_set<int> seen;
        vector<int> unique_ids;

        for (int id : ids) {
            if (seen.insert(id).second) {
                unique_ids.push_back(id);  
            }
        }
        ids = move(unique_ids); 
    }
}

// 统计base容器和delta容器的个数
pair<int, int> ContainerCache::countBaseAndDelta(uint64_t threshold) {
    // 小于或等于 threshold 的容器是base
    int count_base = 0, count_delta = 0;
    if (this->reference_containers.find(CONTAINER) != reference_containers.end()){
        count_base = count_if(this->reference_containers[CONTAINER].begin(), this->reference_containers[CONTAINER].end(),
                                    [threshold](uint64_t value) { return value <= threshold; });
        count_delta = count_if(this->reference_containers[CONTAINER].begin(), this->reference_containers[CONTAINER].end(),
                                        [threshold](uint64_t value) { return value > threshold; });
    }
    if (this->reference_containers.find(HOT_CONTAINER) != reference_containers.end()){
        count_base += this->reference_containers[HOT_CONTAINER].size();
    }
    if (this->reference_containers.find(COLD_CONTAINER) != reference_containers.end()){
        count_base += this->reference_containers[COLD_CONTAINER].size();
    }
    
    return {count_base, count_delta};
}


void ContainerCache::printContainers(int base_container_max_value){
    // 读取容器的次数
    int container_read_count = this->getReferenceContainerCount();
    printf("Read Container Count: %d\n", container_read_count);
    // auto [base_counter, delta_container] = this->countBaseAndDelta(base_container_max_value);
    // printf("Read Base Container Count: %d\n", base_counter);
    // printf("Read Delta Container Count: %d\n", delta_container);

    // // 去重后是引用容器的个数
    this->removeDuplicates();
    int reference_containers_count = this->getReferenceContainerCount(); 
    printf("Reference Container Count: %d\n", reference_containers_count);
    // auto [r_base_counter, r_delta_container] = this->countBaseAndDelta(base_container_max_value);
    // printf("Reference Base Container Count: %d\n", r_base_counter);
    // printf("Reference Delta Container Count: %d\n", r_delta_container);
}