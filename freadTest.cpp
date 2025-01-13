#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <cstring>
#include <cstdlib>

int main() {
    const std::string filePath = "/home/jyc/ssd/restore/restore0";

    // 打开文件
    int fd = open(filePath.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0777);
    if (fd == -1) {
        std::cerr << "open failed: " << strerror(errno) << std::endl;
        return -1;
    }

    // 分配对齐的缓冲区（假设块大小是 512）
    const size_t buffer_size = 256*1024*1024; // 确保是块大小的倍数
    void* buffer;
    if (posix_memalign(&buffer, 512, buffer_size) != 0) {
        std::cerr << "posix_memalign failed" << std::endl;
        close(fd);
        return -1;
    }

    // 写入数据
    const size_t write_size = 256*1024*1024-24;
    memset(buffer, 0, buffer_size); // 示例：填充为 0
    ssize_t written = write(fd, buffer, write_size);
    if (written == -1) {
        std::cerr << "write failed: " << strerror(errno) << std::endl;
    } else {
        std::cout << "write success: " << written << " bytes" << std::endl;
    }

    // 释放资源
    free(buffer);
    close(fd);

    return 0;
}
