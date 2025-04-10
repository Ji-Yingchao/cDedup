#ifndef DEDUP_STATS_H
#define DEDUP_STATS_H

#include <vector>
#include <string>
#include <utility>

// log "attr && dr" 
void saveDedupRatio(bool in_delta, double dr);
std::pair<std::string, double> loadDedupRatioAtLine(int target_line);
std::vector<std::pair<std::string, double>> loadAllDedupRatios();
int findNearestBaseBefore(const std::vector<std::pair<std::string, double>>& dr_vec,int current_index);


// get attr
//std::string getDeltaAttr();
std::vector<std::string> loadDeltaAttrs();

#endif // DEDUP_STATS_H
