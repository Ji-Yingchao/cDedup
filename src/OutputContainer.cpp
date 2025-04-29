#include "OutputContainer.h"
#include "general.h"
#include "metadata.h"
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

OutputContainer::OutputContainer(const string& path_prefix, CONTAINER_TYPE type, int version)
    : pathPrefix_(path_prefix), index_(0), bufPointer_(0),
      innerOffset_(0), innerIndex_(0)
{
    containerBuf_ = new unsigned char[CONTAINER_SIZE];
    memset(containerBuf_, 0, CONTAINER_SIZE);
    container_type = container_type_to_string(type);
    index_ = getVersion(path_prefix.c_str(), container_type);
    version_ = version;
}

OutputContainer::~OutputContainer() {
    if (bufPointer_ > 0) {
        flush();
    }
    delete[] containerBuf_;
}

void OutputContainer::writeChunk(int chunk_length, int file_offset, unsigned char* file_cache, ENTRY_VALUE& entry_value) {
    if (bufPointer_ + chunk_length >= CONTAINER_SIZE) {
        flush();
    }

    memcpy(containerBuf_ + bufPointer_, file_cache + file_offset, chunk_length);
    
    entry_value.container_number = index_;
    entry_value.offset = innerOffset_;
    entry_value.chunk_length = chunk_length;
    entry_value.container_inner_index = innerIndex_;
    // TODO: 版本还未确定
    entry_value.version = version_;
    entry_value.ref_cnt = 1;
    entry_value.container_type = CONTAINER;
    
    bufPointer_ += chunk_length;
    innerOffset_ += chunk_length;
    innerIndex_++;
}

void OutputContainer::writeChunk(const string& chunk_data, ENTRY_VALUE& value) {
    size_t len = chunk_data.size();
    if (bufPointer_ + len >= CONTAINER_SIZE) {
        flush();
    }

    memcpy(containerBuf_ + bufPointer_, chunk_data.data(), len);
    
    // 更新 ENTRY_VALUE
    value.container_number = index_;
    value.offset = innerOffset_;
    value.container_inner_index = innerIndex_;
    value.container_type = string_to_container_type(container_type);
    
    bufPointer_ += len;
    innerOffset_ += len;
    innerIndex_++;
}

void OutputContainer::flush() {
    saveContainer(index_, containerBuf_, bufPointer_, pathPrefix_.c_str());
    memset(containerBuf_, 0, CONTAINER_SIZE);

    index_++;
    bufPointer_ = 0;
    innerOffset_ = 0;
    innerIndex_ = 0;
}

void OutputContainer::saveContainer(int container_index, unsigned char* container_buf, unsigned int len, const char* containersPath) {
    std::string container_name = std::string(containersPath) + "/" + container_type + std::to_string(container_index);

    int fd = open(container_name.data(), O_RDWR | O_CREAT, 0777);
    if (fd == -1) {
        printf("open error: %s\n", strerror(errno));
        exit(-1);
    }

    if (write(fd, container_buf, CONTAINER_SIZE) != CONTAINER_SIZE) {
        printf("saveContainer write error, id %d, %s\n", errno, strerror(errno));
        close(fd);
        exit(-1);
    }

    close(fd);
}
