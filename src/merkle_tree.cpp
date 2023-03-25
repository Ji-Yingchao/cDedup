#include "merkle_tree.h"
#include <openssl/sha.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

class Chunk;

void MerkleTree::showTreeSize(){
    for(int i=0; i<=this->tree.size()-1; i++){
        printf("L%d has %d nodes\n", i+1, this->tree[i].size());
    }
}

bool MerkleTree::buildLevelx(int p){
    if(this->tree[p-2].size() == 1){
        //已经到头了，这个下层只有一个节点，无法再构建一层了
        return false;
    }

    // 利用i-1层的节点构建第i层节点
    SHA_CTX c;
    unsigned char sha1[SHA_DIGEST_LENGTH];
    int len = 20;
    int num  = 0;
    for(int i=0; i<=this->tree[p-2].size()-1; i++){
        SHA1_Init(&c);
        memset(sha1, 0, SHA_DIGEST_LENGTH);
        SHA1_Update(&c, this->tree[p-2][i]->SHA1_hash.c_str(), len);
        num++;
        if(num % this->group_size == 0){
            SHA1_Final(sha1, &c);
            num = 0;
            this->tree[p-1].emplace_back(new LP_node(sha1, p));
        
            for(int j=this->group_size; j>=1; j--){
                this->tree[p-1].back()->children.emplace_back(this->tree[p-2][i-j+1]);
            }
        }
    }
    if(num == 0){
        return true;
    }

    SHA1_Final(sha1, &c);
    this->tree[p-1].emplace_back(new LP_node{sha1, p});
    for(int j=num; j>=1; j--){
        this->tree[p-1].back()->children.emplace_back(this->tree[p-2][this->tree[p-2].size()-j]);
    }
    return true;
}

void MerkleTree::buildLevel1(std::vector<L0_node>& nodes){
    SHA_CTX c;
    unsigned char sha1[SHA_DIGEST_LENGTH];
    int len = 20;
    int num  = 0;
    for(int i=0; i<=nodes.size()-1; i++){
        SHA1_Init(&c);
        memset(sha1, 0, SHA_DIGEST_LENGTH);
        SHA1_Update(&c, nodes[i].SHA1_hash.c_str(), len);
        num++;
        if(num % this->group_size == 0){
            // 计算上层hash
            SHA1_Final(sha1, &c);
            num = 0;
            this->tree[0].emplace_back(new LP_node(sha1, 1));
        
            // 上层节点链接下层
            for(int j=this->group_size; j>=1; j--){
                this->tree[0].back()->children.emplace_back(&nodes[i-j+1]);
            }
        }
    }
    // 计算上层hash，这层剩余的节点可能不够group size，但是还是凑一起
    SHA1_Final(sha1, &c);
    this->tree[0].emplace_back(new LP_node{sha1, 1});

    // 上层节点链接下层
    for(int j=num; j>=1; j--){
        this->tree[0].back()->children.emplace_back(&nodes[nodes.size()-j]);
    }
}

void MerkleTree::buildTree(std::vector<L0_node>& nodes){
    // 构建Li层的节点
    for(int i=1; i<=this->max_level; i++){
        if(i==1){
            buildLevel1(nodes);
        }else{
            if(!buildLevelx(i))
                break;
        }
    }
}

void MerkleTree::markNonDuplicateNodes(){
    if(this->tree.back().size() > 1){
        // 默认的L6层节点不止一个，直接报错
        printf("ERROR, L6 size > 1\n");
        this->showTreeSize();
        exit(-1);
    }
    
    for(int i=this->max_level-1; i>=0; i--){
        if(this->tree[i].size() == 1){
            this->walk(this->tree[i].back());
            break;
        }
    }
}

void MerkleTree::walk(LP_node* root){
    if(root->level == 0){
        if(lookupL0(root->SHA1_hash.c_str())){
            ;
        }else{
            root->found = false;
        }
    }else{
        if(lookupLP(root->SHA1_hash.c_str(), root->level)){
            ;
        }else{
            root->found = false;
            this->insertLP(root->SHA1_hash.c_str(), root->level);
            for(int i=0; i<=root->children.size()-1; i++){
                walk(root->children[i]);
            }
        }
    }
}

// 和main里面的元数据长度一样
# define L0_META_DATA_SIZE 30 
bool MerkleTree::lookupL0(const char* hash){
    int fd = open(this->meta_path[0], O_RDONLY, 777);
    int hash_cache_length = L0_META_DATA_SIZE*1024*1024;
    char *hash_cache = (char*)malloc(sizeof(char)*hash_cache_length);
    int n = read(fd, hash_cache, hash_cache_length);
    
    for(int i=0; i<=n/L0_META_DATA_SIZE - 1; i++){
        for(int j=0; j<=SHA_DIGEST_LENGTH-1; j++){
            if(hash[j] != hash_cache[j + i*L0_META_DATA_SIZE])
                break;
            if(j == SHA_DIGEST_LENGTH-1){
                close(fd);
                return true;
            }
        }
    }

    close(fd);
    free(hash_cache);
    return false;
}

// char和unsigned char不能比较！！！
bool MerkleTree::lookupLP(const char* hash, int P){
    int fd = open(this->meta_path[P], O_RDONLY, 777);
    int hash_cache_length = SHA_DIGEST_LENGTH*1024*1024;
    char *hash_cache = (char*)malloc(sizeof(char)*hash_cache_length);
    int n = read(fd, hash_cache, hash_cache_length);
    
    for(int i=0; i<=n/SHA_DIGEST_LENGTH - 1; i++){
        for(int j=0; j<=SHA_DIGEST_LENGTH-1; j++){
            if(hash[j] != hash_cache[j + i*SHA_DIGEST_LENGTH])
                break;
            if(j == SHA_DIGEST_LENGTH-1){
                close(fd);
                return true;
            }
        }
    }

    close(fd);
    free(hash_cache);
    return false;
}

void MerkleTree::insertLP(const char* hash, int level){
    int fd = open(this->meta_path[level], O_RDWR | O_APPEND, 777);
    write(fd, hash, SHA_DIGEST_LENGTH);
    close(fd);
}