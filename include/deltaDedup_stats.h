#ifndef DEDUP_STATS_H
#define DEDUP_STATS_H

#include <vector>
#include <string>
using namespace std;


enum FILE_ATTR{
    ATTR_BASE,      
    ATTR_SLBASE,    //与base重删，之后的delta与它重删
    ATTR_DELTA,      
};

string attr_to_string(FILE_ATTR attr);
FILE_ATTR string_to_attr(const string& str);

// get attr
vector<FILE_ATTR> loadDeltaAttrs();
//string getDeltaAttr();


struct AttrWithDR {
    FILE_ATTR attr;
    double dedup_ratio;

    // 构造函数
    AttrWithDR(const FILE_ATTR& a, double ratio) : attr(a), dedup_ratio(ratio) {}
};


// log "attr && dr" 
void saveDedupRatio(FILE_ATTR file_attr, double dr);
AttrWithDR loadDedupRatioAtLine(int target_line);
vector<AttrWithDR> loadAllDedupRatios();
pair<int, int> findNearestBaseBefore(const vector<AttrWithDR>& dr_vec,int current_index);


// log container index sequence
void saveContainerIds(vector<int> refContainers, int current_version);
vector<int> loadContainerIds(int current_version);



#endif // DEDUP_STATS_H
