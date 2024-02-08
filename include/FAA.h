#include<iostream>
#include<general.h>

// 参考FAST13 FAA论文 table1的结构
class recipe_buffer_entry{
    public:
        int CID;
        int length;
        int container_offset;
        int faa_start;
        bool used;

        recipe_buffer_entry(int CID_, int length_, int container_offset_, int faa_start_, bool used_){
            CID = CID_;
            length = length_;
            container_offset = container_offset_;
            faa_start = faa_start_;
            used = used_;
        }
};

class FAA{
    public:
        static void loadContainer(int CID, std::string containers_path, unsigned char* container_read_buffer);
};

void FAA::loadContainer(int CID, std::string containers_path, unsigned char* container_read_buffer){
    std::string container_name(containers_path);
    container_name.append("/container");
    container_name.append(std::to_string(CID));
    int fd = open(container_name.data(), O_RDONLY);

    memset(container_read_buffer, 0, CONTAINER_SIZE);
    read(fd, container_read_buffer, CONTAINER_SIZE); // 可能塞不满
    close(fd);
}