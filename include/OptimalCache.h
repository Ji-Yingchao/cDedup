#include "MetadataManager.h"
#include "Cache.h"
#include "config.h"
#include "deltaDedup_stats.h"
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <queue>

// #define SECTOR_SIZE (512)
#define SECTOR_SIZE (4096)
class OptimalCache : public Cache{
    public:
        OptimalCache(const char* containersPath, int cache_max_size){
            this->containers_path = containersPath;
            this->cache_max_size = cache_max_size; // 单位：容器数量
            posix_memalign((void**)&this->container_buf, SECTOR_SIZE, CONTAINER_SIZE);

            this->previous_index.containerId = -1;
            this->hot_containers_path = Config::getInstance().getHotContainersPath();
        }

        ~OptimalCache(){
            free(this->container_buf);
        }
        
        virtual string getChunkData(ENTRY_VALUE ev);

        void initializeAccessSequence(const vector<ContainerKey>& sequence);

        int getReferenceContainerCount();

        void printContainers(int base_container_max_value);

    private:
        unordered_set<ContainerKey, ContainerKeyHash> container_index_set;
        unordered_map<ContainerKey, string, ContainerKeyHash> cache;
        string containers_path;
        int cache_max_size;
        char* container_buf;
        
        unordered_map<ContainerKey, vector<int>, ContainerKeyHash> future_access_map; // 每个容器未来访问的位置
        ContainerKey previous_index;

        string hot_containers_path;

        void loadContainer(uint32_t container_number, CONTAINER_TYPE container_type);
        void evictContainerOptimal();

        // 恢复时引用的容器
        unordered_map<CONTAINER_TYPE, vector<int>> reference_containers; 
        unordered_map<CONTAINER_TYPE, int>  average_containers;  

        void removeDuplicates();
        pair<int, int> countBaseAndDelta(uint64_t threshold);
};