#ifndef CONTAINER_H
#define CONTAINER_H

#include <unistd.h>
#include <string.h>
#include <sstream>

/** 容器类型 */
enum CONTAINER_TYPE{
    HOT_CONTAINER,   //热容器   
    COLD_CONTAINER,  //冷容器
    CONTAINER,       //未分类的容器
};


struct ContainerKey {
    CONTAINER_TYPE type;
    int containerId;

    // 重载 ==，unordered_map 需要
    bool operator==(const ContainerKey& other) const {
        return type == other.type && containerId == other.containerId;
    }
};

struct ContainerKeyHash {
    size_t operator()(const ContainerKey& k) const {
        return std::hash<int>()(static_cast<int>(k.type)) ^ (std::hash<int>()(k.containerId) << 1);
    }
};

std::string container_type_to_string(CONTAINER_TYPE type);
CONTAINER_TYPE string_to_container_type(const std::string& str);

#endif // CONTAINER_H



// reference_containers是unordered_map，需要hash值
// namespace std {
//     template <>
//     struct hash<CONTAINER_TYPE> {
//         size_t operator()(const CONTAINER_TYPE& type) const {
//             return static_cast<size_t>(type);
//         }
//     };
// }