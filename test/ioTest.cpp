#include <iostream>
#include <cstring> 
#include <cstdio>
#include <chrono> // 用于精确测量时间
#include <unistd.h>
#include <fcntl.h>

void test_read_performance(const char* filename, size_t buffer_size) {
    int fd = open(filename, O_RDONLY | O_DIRECT);
    if (fd == -1) {
        perror("Failed to open file");
        return ;
    }

    void *buffer; // 分配缓冲区
    if (posix_memalign(&buffer, 512, buffer_size) != 0) {
        perror("Failed to allocate aligned memory");
        close(fd);
        return ;
    }

    // 记录开始时间
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t total_bytes_read = 0;
    size_t bytes_read;

    // 读取文件
    while ((bytes_read = read(fd, buffer, buffer_size)) > 0) {
        total_bytes_read += bytes_read;
    }
    // 记录结束时间
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    free(buffer);
    close(fd);

    // 输出结果
    std::cout << "[读取性能测试]" << std::endl;
    std::cout << "读取的总字节数: " << total_bytes_read << " bytes" << std::endl;
    std::cout << "耗时: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "吞吐量: " << (total_bytes_read / (1024.0 * 1024.0)) / elapsed.count() << " MB/s" << std::endl;
}

void test_write_performance(const char* filename, size_t buffer_size, size_t file_size_mb) {
    int fd = open(filename, O_RDWR | O_CREAT | O_DIRECT, 0777);
    if (fd == -1) {
        perror("Failed to open file");
        return ;
    }

    void *buffer;
    if (posix_memalign(&buffer, 512, buffer_size) != 0) {
        perror("Failed to allocate aligned memory");
        close(fd);
        return ;
    }

    for (size_t i = 0; i < buffer_size; i++) {
        ((char*)buffer)[i] = (char)(i % 256);
    }

    // 记录开始时间
    auto start_time = std::chrono::high_resolution_clock::now();

    size_t total_bytes_written = 0;
    size_t bytes_to_write = file_size_mb * 1024 * 1024;

    // 写入文件
    while (total_bytes_written < bytes_to_write) {
        size_t bytes_written = write(fd, buffer, buffer_size);
        if (bytes_written == -1) {
            perror("Failed to write to file");
            free(buffer);
            close(fd);
            return ;
        }
        total_bytes_written += bytes_written;
    }

    // 记录结束时间
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    free(buffer);
    close(fd);

    // 输出结果
    std::cout << "[写入性能测试]" << std::endl;
    std::cout << "写入的总字节数: " << total_bytes_written << " bytes" << std::endl;
    std::cout << "耗时: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "吞吐量: " << (total_bytes_written / (1024.0 * 1024.0)) / elapsed.count() << " MB/s" << std::endl;
}

int main() {
    // /home/jyc/ssd/dataset/linuxVersion/linux-5.10.100.tar
    // /home/jyc/ssd/restore/test_write.dat
    // /home/jyc/hdd/data/linux-5.10.100.tar
    // /home/jyc/hdd/restore/test_write.dat
    const char* read_filename = "/home/jyc/hdd/data/linux-5.10.100.tar"; // 要读取的文件
    const char* write_filename = "/home/jyc/hdd/restore/test_write.dat";    // 要写入的文件
    const size_t buffer_size = 30 * 4 * 1024 * 1024;                            // 30*4MB的缓冲区
    const size_t file_size_mb = 1024;                                           // 写入文件大小 (MB)

    // 测试读性能
    test_read_performance(read_filename, buffer_size);

    // 测试写性能
    test_write_performance(write_filename, buffer_size, file_size_mb);

    // 删除写测试文件
    if (std::remove(write_filename) == 0) {
        std::cout << "写测试文件已删除: " << write_filename << std::endl;
    } else {
        std::cerr << "无法删除写测试文件: " << write_filename << std::endl;
    }

    return 0;
}
