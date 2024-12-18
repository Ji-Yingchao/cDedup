// #include <iostream>
// #include <cstdio>
// #include <ctime>  // 用于测量时间
// #include <chrono> // 用于更精确的时间测量

// int main() {
//     const char* filename = "/home/jyc/ssd/dataset/linuxVersion/linux-5.10.100.tar"; // 要读取的文件
//     const size_t buffer_size = 30 * 4 * 1024 * 1024; // 30*4MB的缓冲区
//     char* buffer = new char[buffer_size];   // 分配缓冲区

//     FILE* file = fopen(filename, "rb");
//     if (!file) {
//         std::cerr << "无法打开文件: " << filename << std::endl;
//         return 1;
//     }

//     // 记录开始时间
//     auto start_time = std::chrono::high_resolution_clock::now();

//     size_t total_bytes_read = 0;
//     size_t bytes_read;
    
//     // 读取文件
//     while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
//         total_bytes_read += bytes_read;
//     }

//     // 记录结束时间
//     auto end_time = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double> elapsed = end_time - start_time;

//     fclose(file);
//     delete[] buffer;

//     // 输出结果
//     std::cout << "读取的总字节数: " << total_bytes_read << " bytes" << std::endl;
//     std::cout << "耗时: " << elapsed.count() << " seconds" << std::endl;
//     std::cout << "吞吐量: " << (total_bytes_read / (1024 * 1024)) / elapsed.count() << " MB/s" << std::endl;

//     return 0;
// }
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
