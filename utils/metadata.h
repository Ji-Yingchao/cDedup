#ifndef VERSION_UTILS_H
#define VERSION_UTILS_H

#include <string>
#include <vector>

// 从目录中找出下一个版本号
int getVersion(const char* dirPath, const std::string& prefix);

// 提取一个 fingerprint 文件中所有涉及的 container ID
std::vector<uint32_t> getContainerIds(std::string fp_name, uint64_t file_size);

#endif // VERSION_UTILS_H
