#include"MetadataManager.h"
#include<unordered_set>

class ContainerCache{
    public:
        ContainerCache(char* containersPath, int cache_size){
            this->containers_path = containersPath;
            this->cache_size = cache_size; // 单位：容器数量
        }
        std::string getChunkData(ENTRY_VALUE ev);

    private:
        std::unordered_set<int> container_number_set;
        std::string containers_path;
        int cache_size;
};