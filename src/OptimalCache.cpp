#include "OptimalCache.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

string OptimalCache::getChunkData(ENTRY_VALUE ev){
    ContainerKey key = {ev.container_type, ev.container_number};

    //刷新future_access_map，当前index与上个版本容器index不一样时erase
    if(this->previous_index.containerId == -1 || this->previous_index != key){
        //future_access_map[key].erase(future_access_map[key].begin());
        auto& vec = future_access_map[key];
        if (!vec.empty()) {
            vec.erase(vec.begin()); // 更新未来访问位置
        }else{
            printf("当前访问容器索引: %d\n", *vec.begin());
            exit(-1);
        }
        this->previous_index = key;
    }

    auto numberIter = this->container_index_set.find(key);
    if(numberIter != this->container_index_set.end()){
        // cache hit
        return string(cache[key], ev.offset, ev.chunk_length);
    } else {
        // cache miss
        if(container_index_set.size() >= cache_max_size){
            evictContainerOptimal(); // 替换为最优策略
        }

        loadContainer(ev.container_number, ev.container_type);
        return string(cache[key], ev.offset, ev.chunk_length);
    }
}


void OptimalCache::loadContainer(uint32_t container_index, CONTAINER_TYPE container_type){
    ContainerKey key = {container_type, container_index};
    container_index_set.insert(key);

    string container_path = (container_type == HOT_CONTAINER) ? hot_containers_path : this->containers_path;
    string container_name = container_path + "/" + container_type_to_string(container_type) + to_string(container_index);

    int fd = open(container_name.data(), O_RDONLY | O_DIRECT);
    if (fd == -1) {
        printf("open container error: %s\n", strerror(errno));
        printf("container path: %s\n", container_name.c_str());
        exit(-1);
    }

    memset(this->container_buf, 0, CONTAINER_SIZE);
    int n = read(fd, this->container_buf, CONTAINER_SIZE);
    string content(this->container_buf , n);
    this->cache[key] = content;

    this->reference_containers[container_type].push_back(container_index);
    //printf("%d\n", container_index);

    close(fd);
}


void OptimalCache::evictContainerOptimal() {
    int latest_use = -1;
    ContainerKey to_evict;
    bool found = false;

    for (const auto& key : container_index_set) {
        auto& future_list = future_access_map[key];
        if (future_list.empty()) {
            // 永不再使用，直接淘汰
            to_evict = key;
            found = true;
            break;
        } else {
            if (future_list[0] > latest_use) {
                latest_use = future_list[0];
                to_evict = key;
                found = true;
            }
        }
    }

    if (found) {
        this->container_index_set.erase(to_evict);
        this->cache.erase(to_evict);
    } else {
        // 理论上不应走到这里
        fprintf(stderr, "No container to evict found!\n");
        exit(1);
    }
}

void OptimalCache::initializeAccessSequence(const vector<ContainerKey>& access_sequence){
    for (int i = 0; i < access_sequence.size(); ++i) {
        ContainerKey key = {
            access_sequence[i].type,
            access_sequence[i].containerId
        };
        this->future_access_map[key].push_back(i);
    }
}



// 统计恢复时的容器数量
int OptimalCache::getReferenceContainerCount(){
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
void OptimalCache::removeDuplicates() {
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
pair<int, int> OptimalCache::countBaseAndDelta(uint64_t threshold) {
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


void OptimalCache::printContainers(int base_container_max_value){
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