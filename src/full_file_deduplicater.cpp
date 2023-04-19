#include "full_file_deduplicater.h"
#include <openssl/sha.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>

/*
    计算全文件哈希
    暂时不能计算超过FILE_CACHE大小的文件
*/
void FullFileDeduplicater::compute_file_hash(char* input_file_path, unsigned char* hash){
    unsigned char * full_file_cache = (unsigned char *)malloc(FILE_CACHE);
    int fd = open(input_file_path, O_RDONLY, 777);
    int n_read = read(fd, full_file_cache, FILE_CACHE);
    SHA1(full_file_cache, n_read, hash);
    close(fd);
}

/*
    扫描指纹文件，每20字节对比，如果找到匹配，那么该文件是重复的。
*/
void FullFileDeduplicater::scan_and_insert_hash(const unsigned char* hash){
    int fd = open(this->fingerprint_path.c_str(), O_RDONLY, 777);
    unsigned char file_fingerprints_cache[FILE_FINGERPRINTS_CACHE] = {0};
    read(fd, file_fingerprints_cache, FILE_FINGERPRINTS_CACHE);
    for(int i=0; i<=this->files_num-1; i++){
        for(int j=0; j<=HASH_LENGTH-1; j++){
            if(hash[j] != file_fingerprints_cache[j + i*HASH_LENGTH])
                break;
            if(j == HASH_LENGTH-1){
                this->file_id = i; // 找到编号为i的hash
                this->file_exist = true;
                close(fd);
                return ;
            }
        }
    }
    this->file_id = files_num;
    this->file_exist = false;
    this->insert_hash(hash);
    close(fd);
}

void FullFileDeduplicater::insert_hash(const unsigned char* hash){
    int fd = open(this->fingerprint_path.c_str(), O_RDWR | O_APPEND, 777);
    write(fd, hash, HASH_LENGTH);
    close(fd);
}

/*
    统计已经存储了几个文件（重删后的）
    默认使用SHA1，每个指纹20字节
*/
void FullFileDeduplicater::init_files_num(){
    int fd = open(this->fingerprint_path.c_str(), O_RDONLY, 777);
    struct stat _stat;
    fstat(fd, &_stat);
    this->files_num = _stat.st_size / (SHA_DIGEST_LENGTH); 
    close(fd);
}

void FullFileDeduplicater::restoreFile(int file_id, char* restore_path){
    std::string abs_saved_file_name = generate_abs_file_name_from_file_id(this->storage_path, file_id); 
    int fd_new = open(restore_path, O_CREAT | O_RDWR, 777);
    int fd_saved = open(abs_saved_file_name.c_str(), O_RDONLY, 777);
    sendfile(fd_new, fd_saved, NULL, FULL_FILE_CACHE);
}

/*
    返回文件ID，后续恢复时需要使用。
*/
void FullFileDeduplicater::writeFile(char* input_file_path){
    unsigned char hash[HASH_LENGTH] = {0};
    this->compute_file_hash(input_file_path, hash);
    this->scan_and_insert_hash(hash);
    if(!this->file_exist)
        this->save_file(input_file_path);
}

bool FullFileDeduplicater::get_file_exist(){
    return this->file_exist;
}

int FullFileDeduplicater::get_file_id(){
    return this->file_id;
}

void FullFileDeduplicater::save_file(const char* input_file_path){
    std::string abs_file_name = generate_abs_file_name_from_file_id(this->storage_path, this->file_id); 
    int fd_new = open(abs_file_name.c_str(), O_CREAT | O_RDWR, 777);
    int fd_input = open(input_file_path, O_RDONLY, 777);
    sendfile(fd_new, fd_input, NULL, FULL_FILE_CACHE);
}

std::string FullFileDeduplicater::generate_abs_file_name_from_file_id(std::string _storage_path, int _file_id){
    std::string abs_file_name="";
    if(_storage_path.back() == '/'){
        abs_file_name = _storage_path + std::to_string(file_id);
    }
    else {
        abs_file_name.append(_storage_path);
        abs_file_name.push_back('/');
        abs_file_name.append(std::to_string(_file_id));
    }
    return abs_file_name;
}