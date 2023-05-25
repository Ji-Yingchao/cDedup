#include"MetadataManager.h"
#include"Cache.h"
#include<unordered_set>
#include<unordered_map>
#include<vector>
#include<queue>

class ContainerCache : public Cache{
    public:
        ContainerCache(const char* containersPath, int cache_max_size){
            this->containers_path = containersPath;
            this->cache_max_size = cache_max_size; // 单位：容器数量
            this->container_buf = (char*)malloc(CONTAINER_SIZE);
        }

        ~ContainerCache(){
            free(this->container_buf);
        }
        
        virtual std::string getChunkData(ENTRY_VALUE ev);

    private:
        std::unordered_set<int> container_index_set;
        std::queue<int> container_index_queue;
        std::string containers_path;
        int cache_max_size;
        std::unordered_map<int, std::string> cache;
        char* container_buf;

        void loadContainer(int container_number);
        void evictContainerFIFO();
};