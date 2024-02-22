#ifndef MATADATA_MANAGER_H
#define MATADATA_MANAGER_H

#include <stdio.h>
#include <iostream>
#include <unordered_map>
#include "general.h"

enum LookupResult {
    Unique,
    Dedup,
};

struct __attribute__ ((__packed__)) SHA1FP {
    // 20 bytes
    uint64_t fp1;
    uint32_t fp2, fp3, fp4;

    void print() {
        printf("%lu:%d:%d:%d\n", fp1, fp2, fp3, fp4);
    }
};

struct ENTRY_VALUE {
    uint32_t container_number;
    uint32_t offset;
    uint16_t chunk_length;
    uint16_t container_inner_index;
};

struct Hasher {
    uint32_t operator()(SHA1FP key) const {
        return key.fp1;
    }
};

struct Equaler {
    bool operator()(const SHA1FP &lhs, const SHA1FP &rhs) const {
        return lhs.fp1 == rhs.fp1 && lhs.fp2 == rhs.fp2 && lhs.fp3 == rhs.fp3 && lhs.fp4 == rhs.fp4;
    }
};

class MetadataManager {
    public:
        MetadataManager(const std::string& file_path) {
            this->metadata_file_path = file_path;
        }

        void save();
        void saveBatch();
        void load();
        LookupResult dedupLookup(SHA1FP *sha1);
        void addNewEntry(const SHA1FP *sha1, const ENTRY_VALUE *value);
        int addRefCnt(const SHA1FP sha1);
        int decRefCnt(const SHA1FP sha1);
        int chunkOffsetDec(SHA1FP sha1, int oft, int len);
        ENTRY_VALUE getEntry(const SHA1FP *sha1);

    private:
        std::string metadata_file_path;
        // FP-index
        std::unordered_map<SHA1FP, ENTRY_VALUE, Hasher, Equaler> fp_table_origin;
        std::unordered_map<SHA1FP, ENTRY_VALUE, Hasher, Equaler> fp_table_added;
};
#endif