#ifndef CONTAINER_H
#define CONTAINER_H

#include <unistd.h>
#include <string.h>
#include <sstream>
#include <vector>

extern int CONTAINER_SIZE;

void set_container_size(int size); 

/** 容器类型 */
enum CONTAINER_TYPE{
    HOT_CONTAINER,   //热容器   
    COLD_CONTAINER,  //冷容器
    CONTAINER,       //未分类的容器
};

std::string container_type_to_string(CONTAINER_TYPE type);

CONTAINER_TYPE string_to_container_type(const std::string& str);


/** 容器键 */
struct ContainerKey {
    CONTAINER_TYPE type;
    uint32_t containerId;

    // 重载 ==，unordered_map 需要
    bool operator==(const ContainerKey& other) const {
        return type == other.type && containerId == other.containerId;
    }

    bool operator!=(const ContainerKey& other) const {
        return type != other.type || containerId != other.containerId;
    }
};

struct ContainerKeyHash {
    size_t operator()(const ContainerKey& k) const {
        return std::hash<int>()(static_cast<int>(k.type)) ^ (std::hash<int>()(k.containerId) << 1);
    }
};



// log container index sequence
void saveContainerIndex(std::vector<ContainerKey> refContainers, int current_version);

std::vector<ContainerKey> loadContainerIds(int current_version);

#endif // CONTAINER_H
