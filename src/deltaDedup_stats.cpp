#include "deltaDedup_stats.h"
#include "config.h"
#include <fstream>
#include <string.h>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>


void saveDedupRatio(FILE_ATTR file_attr, double dr) {
    string attr = attr_to_string(file_attr);
    string dedup_ratio_file = Config::getInstance().getDedupRatioFilePath();
    int fd = open(dedup_ratio_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (fd < 0) {
        printf("saveDedupRatio open error, id %d, %s\n", errno, strerror(errno)); 
        exit(-1);
    }

    // 将 double 转为字符串，加换行符
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "%s\t %.4f\n", attr.c_str(),dr);
    if (write(fd, buf, len) < 0) {
        printf("saveDedupRatio write error, id %d, %s\n", errno, strerror(errno));
        close(fd);
        exit(-1);
    }

    close(fd);
}

AttrWithDR loadDedupRatioAtLine(int target_line) {
    string dedup_ratio_file = Config::getInstance().getDedupRatioFilePath();
    ifstream infile(dedup_ratio_file);
    if (!infile.is_open()) {
        cerr << "loadDedupRatioAtLine open error" << endl;
        exit(-1);
    }

    string line,attr;
    FILE_ATTR file_attr;
    int current_line = 0;
    double dr;

    while (getline(infile, line)) {
        if (!line.empty()) {
            if (current_line == target_line) {
                stringstream ss(line);
                ss >> attr >> dr;
                if (ss.fail()) {
                    cerr << "Failed to parse line " << target_line << ": " << line << endl;
                    exit(-1);
                }
                file_attr = string_to_attr(attr);
                return {file_attr, dr};
            }
            current_line++;
        }
    }

    infile.close();
    cerr << "Line " << target_line << " not found in file." << endl;
    exit(-1);
}


vector<AttrWithDR> loadAllDedupRatios() {
    string dedup_ratio_file = Config::getInstance().getDedupRatioFilePath();
    ifstream infile(dedup_ratio_file);
    if (!infile.is_open()) {
        cerr << "loadAllDedupRatios open error" << endl;
        exit(-1);
    }

    vector<AttrWithDR> results;
    string line;
    string attr;
    double dr;
    FILE_ATTR file_attr;
    while (getline(infile, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        ss >> attr >> dr;
        if (ss.fail()) {
            cerr << "Failed to parse line: " << line << endl;
            continue;  // 可选：跳过错误行而不是退出
        }
        file_attr = string_to_attr(attr);
        results.emplace_back(file_attr, dr);
    }
    infile.close();
    return results;
}

// 找到上一个small base和 base (-1就是对应的值没找到)
pair<int, int> findNearestBaseBefore(const vector<AttrWithDR>& dr_vec, int current_index) {
    int slbase_index = -1; 
    for (int i = current_index; i >= 0; --i) {   // i = current_index-1
        if (dr_vec[i].attr == ATTR_SLBASE && slbase_index == -1) {
            slbase_index = i;  // 只记录第一个遇到的 slbase
        }
        if (dr_vec[i].attr == ATTR_BASE) {
            return {slbase_index, i};  // 第一个是 slbase，第二个是 base
        }
    }
    return {slbase_index, -1};  // 没有找到 base，只返回 slbase（可能是 -1）
}


vector<FILE_ATTR> loadDeltaAttrs(){
    string deltaConfigFilePath = Config::getInstance().getDeltaConfigFilePath();
    ifstream infile(deltaConfigFilePath);  
    vector<FILE_ATTR> attrs;
    string line,attr;
    FILE_ATTR file_attr;
    if (!infile) {
        cerr << "loadDeltaAttrs open error" << endl;
        exit(-1);
    }

    while (getline(infile, line)) {
        stringstream ss(line);
        ss >> attr;
        file_attr = string_to_attr(attr);
        attrs.push_back(file_attr);  
    }

    infile.close();
    return attrs;
}


// 枚举值转字符串
string attr_to_string(FILE_ATTR attr) {
    switch (attr) {
        case ATTR_BASE: return "base";
        case ATTR_SLBASE: return "slbase";
        case ATTR_DELTA: return "delta";
        default: 
            throw invalid_argument("Unknown FILE_ATTR value");
            exit(-1);
    }
}

// 字符串转枚举值
FILE_ATTR string_to_attr(const string& str) {
    if (str == "base") return ATTR_BASE;
    if (str == "slbase") return ATTR_SLBASE;
    if (str == "delta") return ATTR_DELTA;
    throw invalid_argument("Unknown FILE_ATTR string");
    exit(-1);
}


void saveContainerIds(vector<int> refContainers, int current_version){
    string container_index_path = Config::getInstance().getContainerIndexPath();
    string container_index_name(container_index_path);
    container_index_name.append("/container");
    container_index_name.append(to_string(current_version));

    ofstream outFile(container_index_name);
    if (!outFile) {
        cerr << "Unable to open file: " << container_index_name << "\n";
        return;
    }
    for (int value : refContainers) {
        outFile << value << "\n";
    }

    outFile.close();
}

vector<int> loadContainerIds(int current_version) {
    vector<int> refContainers;

    string container_index_path = Config::getInstance().getContainerIndexPath();
    string container_index_name = container_index_path + "/container" + to_string(current_version);

    ifstream inFile(container_index_name);
    if (!inFile) {
        cerr << "Unable to open file: " << container_index_name << "\n";
        return refContainers; // 返回空 vector
    }

    int value;
    while (inFile >> value) {
        refContainers.push_back(value);
    }

    inFile.close();
    return refContainers;
}


// string getDeltaAttr(int target_line){
//     string deltaConfigFilePath = Config::getInstance().getDeltaConfigFilePath();
//     ifstream infile(deltaConfigFilePath);  
//     string line,attr;
//     int current_line = 0;

//     if (!infile) {
//         cerr << "loadDeltaAttrs open error" << endl;
//         exit(-1);
//     }

//     while (getline(infile, line)) {
//         if(!line.empty()){
//             if (current_line == target_line){
//                 stringstream ss(line);
//                 ss >> attr;
//                 if (ss.fail()) {
//                     cerr << "Failed to parse deltaAttr line " << target_line << ": " << line << endl;
//                     exit(-1);
//                 }
//                 return attr;
//             }
//             current_line++;
//         }
          
//     }

//     infile.close();
//     cerr << "Line " << target_line << " not found in deltaAttr file." << endl;
//     exit(-1);
// }