// full file deduplicater
#include <iostream>
#include "general.h"

#define FULL_FILE_CACHE (1024*1024*1024)
#define FILE_FINGERPRINTS_CACHE (20*100)
#define HASH_LENGTH 20

class FullFileDeduplicater{
    private:
        const std::string storage_path;
        const std::string fingerprint_path;
        int files_num;
        int file_id;
        bool file_exist;
        void compute_file_hash(const char* input_file_path, unsigned char* hash);
        void scan_and_insert_hash(const unsigned char* hash);
        void init_files_num();
        void insert_hash(const unsigned char* hash);
        void save_file(const char* input_file_path);
        std::string generate_abs_file_name_from_file_id(std::string _storage_path, int _file_id);

    public:
        FullFileDeduplicater(std::string sp, std::string fp):
        storage_path(sp), 
        fingerprint_path(fp){
            init_files_num();
            this->file_exist = false;
            this->file_id = -1;
        }
        void restoreFile(int file_id, const char* restore_path);
        void writeFile(const char* input_file_path);
        bool get_file_exist();
        int get_file_id();
};