#include <iostream>
#include <cstdio>
#include <ctime>  // 用于测量时间
#include <chrono> // 用于更精确的时间测量

int main() {
    const char* filename = "/home/jyc/ssd/dataset/linuxVersion/linux-5.10.100.tar"; // 要读取的文件
    const size_t buffer_size = 30 * 4 * 1024 * 1024; // 30*4MB的缓冲区
    char* buffer = new char[buffer_size];   // 分配缓冲区

    FILE* file = fopen(filename, "rb");
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return 1;
    }

    // 记录开始时间
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t total_bytes_read = 0;
    size_t bytes_read;
    
    // 读取文件
    while ((bytes_read = fread(buffer, 1, buffer_size, file)) > 0) {
        total_bytes_read += bytes_read;
    }

    // 记录结束时间
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    fclose(file);
    delete[] buffer;

    // 输出结果
    std::cout << "读取的总字节数: " << total_bytes_read << " bytes" << std::endl;
    std::cout << "耗时: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "吞吐量: " << (total_bytes_read / (1024 * 1024)) / elapsed.count() << " MB/s" << std::endl;

    return 0;
}
