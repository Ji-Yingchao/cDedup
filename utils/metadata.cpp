#include "metadata.h"
#include "general.h"
#include "MetadataManager.h"
#include "backup_job.h"
#include <vector>
#include <string>
#include <regex>
#include <experimental/filesystem>
#include <algorithm>

namespace fs = std::experimental::filesystem;

int getVersion(const char* dirPath, const std::string& prefix){
    std::vector<int> recipe_numbers;
    std::regex recipe_pattern(prefix + R"((\d+))");

    for (const auto& entry : fs::directory_iterator(dirPath)) {
        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_search(filename, match, recipe_pattern)) {
            int number = std::stoi(match[1].str());
            recipe_numbers.push_back(number);
        }
    }

    if (!recipe_numbers.empty()) {
        int last_recipe_number = *std::max_element(recipe_numbers.begin(), recipe_numbers.end());
        return last_recipe_number + 1;
    } else {
        return 0;
    }
}


std::vector<uint32_t> getContainerIds(std::string fp_name, uint64_t file_size){
    unsigned char* metadata_cache = (unsigned char*)malloc(FILE_CACHE);
    int fd = open(fp_name.c_str(), O_RDONLY);
    if(fd < 0){
        printf("Open file %s failed\n", fp_name.c_str());
        exit(-1);
    }
    
    int n = read(fd, metadata_cache, FILE_CACHE);
    int meta_size = sizeof(SHA1FP) + sizeof(ENTRY_VALUE);
    int entry_count = n/meta_size;
    ENTRY_VALUE tmp_value;
    std::vector<uint32_t> ans;

    //统计存储大小
    uint64_t stored_size = 0;

    for(int i=0; i<=entry_count-1; i++){
        memcpy(&tmp_value, metadata_cache+i*meta_size + sizeof(SHA1FP), sizeof(ENTRY_VALUE));
        stored_size += tmp_value.chunk_length;
        auto it = std::find(ans.begin(), ans.end(), tmp_value.container_number);
        if(it == ans.end()){
            ans.push_back(tmp_value.container_number);
        }
    }

    printf("stored container size %.2fGB\n", (float)(ans.size()*CONTAINER_SIZE)/GB);
    printf("stored size %.2fGB\n", (float)(stored_size)/GB);
    
    // 用于统计重删率
    bj.dedup_size = bj.dedup_size - file_size + stored_size;
    
    close(fd);
    free(metadata_cache);
    return ans;
}
