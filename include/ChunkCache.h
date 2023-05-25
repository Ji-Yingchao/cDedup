#include"MetadataManager.h"
#include"Cache.h"
#include<map>
#include<vector>
#include<utility>


class ChunkCache : public Cache{
    public:
        ChunkCache(const char* containersPath, int cache_max_size){
            LRUtimestamp = 0;
            this->containers_path = containersPath;
            this->cache_max_size = cache_max_size; // 单位：块数量
            this->container_buf = (char*)malloc(CONTAINER_SIZE);
        }

        ~ChunkCache(){
            free(this->container_buf);
        }

        inline uint64_t ChunkGlobalID(ENTRY_VALUE ev){
            return (uint64_t(ev.container_number) << 32) + ev.container_inner_index;
        }
        
        virtual std::string getChunkData(ENTRY_VALUE ev);

    private:
        std::string containers_path;
        uint64_t LRUtimestamp;
        size_t cache_max_size;
        std::map<uint64_t, std::pair<std::string, uint64_t> > cache; // ChunkGlobalID -> (ChunkData, LRUtimestamp)
        std::map<uint64_t, uint64_t> LRUstack; // LRUtimestamp -> ChunkGlobalID
        char* container_buf;

        std::string loadChunk(ENTRY_VALUE ev);
        void evictChunkLRU();
};