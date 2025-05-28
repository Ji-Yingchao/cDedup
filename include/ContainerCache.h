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
class ContainerCache : public Cache{
    public:
        ContainerCache(const char* containersPath, int cache_max_size){
            this->containers_path = containersPath;
            this->cache_max_size = cache_max_size; // 单位：容器数量
            posix_memalign((void**)&this->container_buf, SECTOR_SIZE, CONTAINER_SIZE);

            this->hot_containers_path = Config::getInstance().getHotContainersPath();
            this->load_container_size = 0;
        }

        ~ContainerCache(){
            free(this->container_buf);
        }
        
        virtual string getChunkData(ENTRY_VALUE ev);

        int getReferenceContainerCount();
        uint64_t getLoadContainerSize();

        void printContainers(int base_container_max_value);

    private:
        unordered_set<ContainerKey, ContainerKeyHash> container_index_set;
        queue<ContainerKey> container_index_queue;
        string containers_path;
        int cache_max_size;
        unordered_map<ContainerKey, string, ContainerKeyHash> cache;
        char* container_buf;

        string hot_containers_path;

        void loadContainer(uint32_t container_number, CONTAINER_TYPE container_type);
        void evictContainerFIFO();

        // 恢复时引用的容器
        unordered_map<CONTAINER_TYPE, vector<int>> reference_containers; 
        unordered_map<CONTAINER_TYPE, int>  average_chunks;
        uint64_t load_container_size;  

        void removeDuplicates();
        pair<int, int> countBaseAndDelta(uint64_t threshold);
};