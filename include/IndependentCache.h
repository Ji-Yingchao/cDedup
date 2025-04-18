#include "Cache.h"
#include <map>
#include <vector>
#include <queue>

// #define SECTOR_SIZE (512)
#define SECTOR_SIZE (4096)
class IndependentCache : public Cache{
    public:
        IndependentCache(const char* containersPath, int cache_max_size, int base_container_max_value){
            this->containers_path = containersPath;
            this->base_cache_max_size = cache_max_size; // 单位：容器数量
            this->base_container_max_value = base_container_max_value; //base容器最大值
            posix_memalign((void**)&this->container_buf, SECTOR_SIZE, CONTAINER_SIZE);
        }

        ~IndependentCache(){
            free(this->container_buf);
        }
        
        virtual std::string getChunkData(ENTRY_VALUE ev);

        // container
        int getReferenceContainerCount();

        void printContainers(int base_container_max_value);


    private:
        std::string containers_path;
        char* container_buf;
        int base_container_max_value; //区分base or delta

        // base cache
        int base_cache_max_size;
        uint64_t LRUtimestamp;
        std::map<int, std::pair<std::string, uint64_t>> base_cache; //container_index -> (ContainerData, LRUtimestamp)
        std::map<uint64_t, int> LRUstack; // LRUtimestamp -> container_index

        // delta cache
        std::pair<int, std::string> delta_cache;  // container_index, containerData

        void loadBaseContainer(int container_index);
        void loadDeltaContainer(int container_number);
        void evictContainerLRU();

        // 恢复时引用的容器
        std::vector<int> read_base_containers;
        std::vector<int> read_delta_containers;

        void removeDuplicates(std::vector<int>& containers);
};