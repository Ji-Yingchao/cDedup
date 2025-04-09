#ifndef DEDUP_STATS_H
#define DEDUP_STATS_H

#include <vector>
#include <string>
#include <utility>

void saveDedupRatio(bool in_delta, double dr);
std::vector<std::pair<std::string, double>> loadAllDedupRatios();

#endif // DEDUP_STATS_H
