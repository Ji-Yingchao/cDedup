#ifndef DEDUP_STATS_H
#define DEDUP_STATS_H

#include <vector>
#include <string>

// 版本属性
enum FILE_ATTR{
    ATTR_BASE,      
    ATTR_SLBASE,    //与base重删，之后的delta与它重删
    ATTR_DELTA,      
};

// 枚举值与字符串转换
std::string attr_to_string(FILE_ATTR attr);
FILE_ATTR string_to_attr(const std::string& str);


// log "attr && dr" 
void saveDedupRatio(FILE_ATTR file_attr, double dr);
std::pair<std::string, double> loadDedupRatioAtLine(int target_line);
std::vector<std::pair<std::string, double>> loadAllDedupRatios();
std::pair<int, int> findNearestBaseBefore(const std::vector<std::pair<std::string, double>>& dr_vec,int current_index);


// get attr
//std::string getDeltaAttr();
std::vector<std::string> loadDeltaAttrs();

#endif // DEDUP_STATS_H
