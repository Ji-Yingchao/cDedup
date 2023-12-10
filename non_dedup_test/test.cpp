#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <string>

using namespace std;

void write_test(){
    string base_filename = "/home/cyf/dataHDD/linuxVersion2/linux-5.0.";
    int out_num = 0;
    for(int j=1; j<=20; j++){
        string filename = base_filename + to_string(j) + ".tar";
        FILE* fd_in = fopen(filename.c_str(), "rb");
        struct timeval time_start, time_end;
        uint64_t time_used_write;
        string write_folder = "./outfiles/";
        string write_sub_folder = "./outfiles/sub" + to_string(j) + "/";
        mkdir(write_sub_folder.c_str(), 0755);

        string write_path;
        int block_num = 256;
        int block_size = 4*1024*1024;
        char* buf = (char*)malloc(block_size);

        gettimeofday(&time_start, NULL);
        for(; ;){
            write_path = write_sub_folder + to_string(out_num);
            memset(buf, 0, block_size);
            int n_read = fread(buf, 1, block_size, fd_in);
            if(n_read == 0)
                break;
            FILE*  fd_out = fopen(write_path.c_str(), "w");
            fwrite(buf, n_read, 1, fd_out);
            fclose(fd_out);
            out_num++;
        }
        gettimeofday(&time_end, NULL);
        uint64_t time_used = (time_end.tv_sec - time_start.tv_sec) * 1000000 + 
                            time_end.tv_usec - time_start.tv_usec;

        fseek(fd_in, 0, SEEK_END);
        uint64_t file_size = ftell(fd_in);

        float throughput_write = (file_size/(1024*1024)) / ((float)time_used/1000000);
        //printf("file %s, write throughput %.2fMiB/s\n", filename.c_str(), throughput_write);
        printf("%.2f\n",throughput_write);
    }
}

void read_test(){
    string restore_base_filename = "/home/cyf/dataHDD/test/restorefiles/";
    string block_folder_base_filename = "/home/cyf/dataHDD/test/outfiles/sub";
    int out_num = 0;
    for(int j=1; j<=20; j++){
        //新建待恢复的文件
        string filename = restore_base_filename + to_string(j) + ".tar";
        FILE* restore_file = fopen(filename.c_str(), "w");

        struct timeval time_start, time_end;
        uint64_t file_size = 0;
        DIR *dir;
        struct dirent *entry;
        string sub_folder = block_folder_base_filename + to_string(j) + "/";
        dir = opendir(sub_folder.c_str());
        char* buf = (char*)malloc(4*1024*1024);

        // 遍历该文件夹下所有block文件，每个block文件一般是4MiB；
        gettimeofday(&time_start, NULL);
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            string block_abs_path = sub_folder;
            block_abs_path.append(entry->d_name);
            memset(buf, 0, 4*1024*1024);
            FILE* block_file = fopen(block_abs_path.c_str(), "r");
            int n_read = fread(buf, 1, 4*1024*1024, block_file);
            file_size += n_read;
            fwrite(buf, 1, n_read, restore_file);
            fclose(block_file);
        }
        gettimeofday(&time_end, NULL);
        uint64_t time_used = (time_end.tv_sec - time_start.tv_sec) * 1000000 + 
                            time_end.tv_usec - time_start.tv_usec;

        float throughput_write = (file_size/(1024*1024)) / ((float)time_used/1000000);
        printf("%.2f\n",throughput_write);
    }
}

int main(){
    read_test();
    return 0;
}