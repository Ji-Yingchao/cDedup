#ifndef DEDUP_STATS_H
#define DEDUP_STATS_H

#include <vector>
#include <string>
using namespace std;


enum CONTAINER_TYPE{
    HOT_CONTAINER,   //热容器   
    COLD_CONTAINER,  //冷容器
    CONTAINER,       //未分类的容器
};


// reference_containers是unordered_map，需要hash值
namespace std {
    template <>
    struct hash<CONTAINER_TYPE> {
        size_t operator()(const CONTAINER_TYPE& type) const {
            return static_cast<size_t>(type);
        }
    };
}

struct ContainerKey {
    CONTAINER_TYPE type;
    int containerId;

    // 重载 ==，unordered_map 需要
    bool operator==(const ContainerKey& other) const {
        return type == other.type && containerId == other.containerId;
    }
};

// struct ContainerHash {
//     size_t operator()(const pair<CONTAINER_TYPE, int>& p) const {
//         return hash<int>()(static_cast<int>(p.first)) ^ (hash<int>()(p.second) << 1);
//     }
// };

struct ContainerKeyHash {
    size_t operator()(const ContainerKey& k) const {
        return hash<int>()(static_cast<int>(k.type)) ^ (hash<int>()(k.containerId) << 1);
    }
};

string container_type_to_string(CONTAINER_TYPE type);
CONTAINER_TYPE string_to_container_type(const string& str);


enum FILE_ATTR{
    ATTR_BASE,      
    ATTR_SLBASE,    //与base重删，之后的delta与它重删
    ATTR_DELTA,      
};

string attr_to_string(FILE_ATTR attr);
FILE_ATTR string_to_attr(const string& str);


// get attr
vector<string> loadDeltaAttrs();
//string getDeltaAttr();


// log "attr && dr" 
void saveDedupRatio(FILE_ATTR file_attr, double dr);
pair<string, double> loadDedupRatioAtLine(int target_line);
vector<pair<string, double>> loadAllDedupRatios();
pair<int, int> findNearestBaseBefore(const vector<pair<string, double>>& dr_vec,int current_index);


// log container index sequence
void saveContainerIds(vector<int> refContainers, int current_version);
vector<int> loadContainerIds(int current_version);



#endif // DEDUP_STATS_H
