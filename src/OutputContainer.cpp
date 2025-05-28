#include "OutputContainer.h"
#include "general.h"
#include "../utils/metadata.h"
#include "config.h"
#include <cstring>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <stdexcept>

OutputContainer::OutputContainer(const string& path_prefix, CONTAINER_TYPE type, int version)
    : pathPrefix_(path_prefix), index_(0), bufPointer_(0),
      innerOffset_(0), innerIndex_(0), rev_container_cnt(0)
{
    containerBuf_ = new unsigned char[CONTAINER_SIZE];
    memset(containerBuf_, 0, CONTAINER_SIZE);
    rev_container_buf= new unsigned char[CONTAINER_SIZE];
    memset(rev_container_buf, 0, CONTAINER_SIZE);
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

void OutputContainer::writeChunk(int chunk_length, int file_offset, unsigned char* file_cache, ENTRY_VALUE& entry_value, void* SHA_buf) {
    if (bufPointer_ + chunk_length >= CONTAINER_SIZE) {
        flush();
    }

    memcpy(containerBuf_ + bufPointer_, file_cache + file_offset, chunk_length);
    memcpy(rev_container_buf + sizeof(SHA1FP)*rev_container_cnt, SHA_buf, sizeof(SHA1FP));
    
    entry_value.container_number = index_;
    entry_value.offset = innerOffset_;
    entry_value.chunk_length = chunk_length;
    entry_value.container_inner_index = innerIndex_;
    entry_value.version = version_;
    entry_value.ref_cnt = 1;
    entry_value.container_type = CONTAINER;
    entry_value.is_arranged = false;
    
    bufPointer_ += chunk_length;
    innerOffset_ += chunk_length;
    innerIndex_++;

    rev_container_cnt ++;
}

void OutputContainer::writeChunk(const string& chunk_data, ENTRY_VALUE& entry_value) {
    size_t len = chunk_data.size();
    if (bufPointer_ + len >= CONTAINER_SIZE) {
        flush();
    }

    memcpy(containerBuf_ + bufPointer_, chunk_data.data(), len);
    
    // 更新 ENTRY_VALUE
    entry_value.container_number = index_;
    entry_value.offset = innerOffset_;
    entry_value.container_inner_index = innerIndex_;
    entry_value.container_type = string_to_container_type(container_type);
    entry_value.is_arranged = true;
    
    bufPointer_ += len;
    innerOffset_ += len;
    innerIndex_++;
}

void OutputContainer::flush() {
    saveContainer(index_, containerBuf_, bufPointer_, pathPrefix_.c_str());
    memset(containerBuf_, 0, CONTAINER_SIZE);
    memset(rev_container_buf, 0, CONTAINER_SIZE);

    index_++;
    bufPointer_ = 0;
    innerOffset_ = 0;
    innerIndex_ = 0;

    rev_container_cnt = 0;
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

    if(Config::getInstance().getDedupMethod() == DEDUP_GLOBAL){
        container_name.append("r");
        fd = open(container_name.data(), O_WRONLY | O_CREAT, 0777);
        //printf("rev_container_cnt: %d\n",rev_container_cnt);
        write(fd, &rev_container_cnt, sizeof(uint32_t));
        write(fd, rev_container_buf, rev_container_cnt * sizeof(SHA1FP));
        close(fd);

        // 写入阶段
        // ssize_t bytes_written, bytes_read;
        // size_t total_size = rev_container_cnt * sizeof(SHA1FP);
        // fd = open(container_name.data(), O_WRONLY | O_CREAT | O_TRUNC, 0777);
        // if (fd < 0) {
        //     perror("open for write failed");
        //     exit(EXIT_FAILURE);
        // }

        // bytes_written = write(fd, &rev_container_cnt, sizeof(uint32_t));
        // if (bytes_written != sizeof(uint32_t)) {
        //     perror("write rev_container_cnt failed");
        //     close(fd);
        //     exit(EXIT_FAILURE);
        // }

        // bytes_written = write(fd, rev_container_buf, total_size);
        // if (bytes_written != (ssize_t)total_size) {
        //     perror("write rev_container_buf failed");
        //     close(fd);
        //     exit(EXIT_FAILURE);
        // }

        // close(fd);
    }
}
