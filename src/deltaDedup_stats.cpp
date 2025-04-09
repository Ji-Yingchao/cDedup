#include "deltaDedup_stats.h"
#include "config.h"
#include <fstream>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>

using namespace std;

void saveDedupRatio(bool in_delta, double dr) {
    string attr = in_delta ? "delta" : "base";
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

vector<pair<string, double>> loadAllDedupRatios() {
    string dedup_ratio_file = Config::getInstance().getDedupRatioFilePath();
    ifstream infile(dedup_ratio_file);
    if (!infile.is_open()) {
        cerr << "loadAllDedupRatios open error" << endl;
        exit(-1);
    }

    vector<pair<string, double>> results;
    string line;
    while (getline(infile, line)) {
        if (line.empty()) continue;
        string attr;
        double dr;
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