#include "deltaDedup_stats.h"
#include "config.h"
#include <fstream>
#include <string.h>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>

using namespace std;

void saveDedupRatio(FILE_ATTR file_attr, double dr) {
    //string attr = in_delta ? "delta" : "base";
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

pair<string, double> loadDedupRatioAtLine(int target_line) {
    string dedup_ratio_file = Config::getInstance().getDedupRatioFilePath();
    ifstream infile(dedup_ratio_file);
    if (!infile.is_open()) {
        cerr << "loadDedupRatioAtLine open error" << endl;
        exit(-1);
    }

    string line,attr;
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
                return {attr, dr};
            }
            current_line++;
        }
    }

    infile.close();
    cerr << "Line " << target_line << " not found in file." << endl;
    exit(-1);
}


vector<pair<string, double>> loadAllDedupRatios() {
    string dedup_ratio_file = Config::getInstance().getDedupRatioFilePath();
    ifstream infile(dedup_ratio_file);
    if (!infile.is_open()) {
        cerr << "loadAllDedupRatios open error" << endl;
        exit(-1);
    }

    vector<pair<string, double>> results;
    string line;
    string attr;
    double dr;
    while (getline(infile, line)) {
        if (line.empty()) continue;
        // string attr;
        // double dr;
        stringstream ss(line);
        ss >> attr >> dr;
        if (ss.fail()) {
            cerr << "Failed to parse line: " << line << endl;
            continue;  // 可选：跳过错误行而不是退出
        }
        results.emplace_back(attr, dr);
    }
    infile.close();
    return results;
}


int findNearestBaseBefore(const vector<pair<string, double>>& dr_vec, int current_index) {
    for (int i = current_index - 1; i >= 0; --i) {
        if (dr_vec[i].first == "base") {
            return i;  // 找到就返回索引
        }
    }
    return -1;  // 没找到
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
//                 std::stringstream ss(line);
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

vector<string> loadDeltaAttrs(){
    string deltaConfigFilePath = Config::getInstance().getDeltaConfigFilePath();
    ifstream infile(deltaConfigFilePath);  
    vector<string> attrs;
    string line,attr;

    if (!infile) {
        cerr << "loadDeltaAttrs open error" << endl;
        exit(-1);
    }

    while (getline(infile, line)) {
        std::stringstream ss(line);
        ss >> attr;
        attrs.push_back(attr);  
    }

    infile.close();
    return attrs;
}


// 枚举值转字符串
std::string attr_to_string(FILE_ATTR attr) {
    switch (attr) {
        case ATTR_BASE: return "base";
        case ATTR_SLBASE: return "slbase";
        case ATTR_DELTA: return "delta";
        default: throw std::invalid_argument("Unknown FILE_ATTR value");
    }
}

// 字符串转枚举值
FILE_ATTR string_to_attr(const std::string& str) {
    if (str == "base") return ATTR_BASE;
    if (str == "slbase") return ATTR_SLBASE;
    if (str == "delta") return ATTR_DELTA;
    throw std::invalid_argument("Unknown FILE_ATTR string");
}