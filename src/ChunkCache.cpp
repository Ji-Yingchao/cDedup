#include"ChunkCache.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

std::string ChunkCache::getChunkData(ENTRY_VALUE ev){
    uint64_t cid = ChunkGlobalID(ev);
    auto cacheIter = cache.find(cid);
    if(cacheIter != cache.end()){ //cache hit
		LRUstack.erase(cacheIter->second.second);
        LRUtimestamp++;
        LRUstack[LRUtimestamp] = cid;
        cacheIter->second.second = LRUtimestamp;

        return cacheIter->second.first;
    }else{ //cache miss
        if(cache.size() >= cache_max_size){
            evictChunkLRU();
        }
        std::string chunk = loadChunk(ev);
        LRUtimestamp++;
        LRUstack[LRUtimestamp] = cid;
        cache[cid] = std::make_pair(chunk, LRUtimestamp);

        return chunk;
    }
}

std::string ChunkCache::loadChunk(ENTRY_VALUE ev){
    // ev.container_number, ev.offset, ev.chunk_length
    std::string container_name(containers_path);
    container_name.append("/container");
    container_name.append(std::to_string(ev.container_number));
    int fd = open(container_name.data(), O_RDONLY);
    if (fd < 0) {
        printf("open error\n");
    }

    uint32_t ost = lseek(fd, ev.offset, SEEK_SET);
    if (ost != ev.offset) {
        printf("lseek error\n");
    }

    uint16_t n = read(fd, this->container_buf, ev.chunk_length);
    if (n != ev.chunk_length) {
        printf("read error\n");
    }
    std::string content(this->container_buf , ev.chunk_length);

    close(fd);
    return content;
}

void ChunkCache::evictChunkLRU(){
    cache.erase(LRUstack.begin()->second);
    LRUstack.erase(LRUstack.begin());
}