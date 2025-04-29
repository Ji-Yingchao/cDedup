#ifndef OUTPUT_CONTAINER_H
#define OUTPUT_CONTAINER_H

#include "MetadataManager.h"
#include "deltaDedup_stats.h"
#include <string>
#include <cstdint>

class OutputContainer {
public:
    OutputContainer(const string& path_prefix, CONTAINER_TYPE type, int version);
    ~OutputContainer();

    // 写入 chunk：从缓存中复制一段
    void writeChunk(int chunk_length, int file_offset, unsigned char* file_cache, ENTRY_VALUE& value);

    // 重载：直接写入 string
    void writeChunk(const string& chunk_data, ENTRY_VALUE& value);

private:
    string pathPrefix_;
    string container_type;
    int index_;
    size_t bufPointer_;
    uint32_t innerOffset_;
    uint16_t innerIndex_;
    unsigned char* containerBuf_;
    int version_;
    //struct ENTRY_VALUE entry_value;

    void flush(); // 写满后保存当前容器
    void saveContainer(int container_index, unsigned char* container_buf, unsigned int len, const char* containersPath);
};

#endif // OUTPUT_CONTAINER_H
