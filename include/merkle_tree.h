#include <vector>
#include <openssl/sha.h>
#include <string>

class LP_node{ 
    friend class MerkleTree;

    public:
        int level;
        bool found;
        std::string SHA1_hash;
        std::vector<LP_node*> children; //L2-L6的孩子指向LP_node，L1的孩子指向L0_node
        LP_node(char* hash):SHA1_hash(hash, 20){
            this->found = true;
        }
};

class L0_node: public LP_node{ 
    public:
        uint32_t file_offset;  
        uint16_t chunk_length; 
        L0_node(uint32_t fo, uint16_t cl, char * hash):
        LP_node(hash)
        {
            this->file_offset  = fo;
            this->chunk_length = cl;
            this->found = true;
        }
};

class MerkleTree{ 
    private:
        int max_level;
        int cur_level;
        int group_size;
        const char** meta_path;
        std::vector<std::vector<LP_node*>> tree;
        std::vector<L0_node>& base;
        void showTreeSize();
        void buildLevelx(int i);
        void buildLevel1(std::vector<L0_node>& nodes);
        void walk(LP_node*);
        bool lookupL0(const char*);
        bool lookupLP(const char*, int);
        void insertLP(const char*, int);

    public:
        MerkleTree(char** meta_, int gs, int ml, std::vector<L0_node>& base_):
        meta_path(meta_),
        base(base_)
        {
            this->group_size = gs;
            this->max_level = ml;
            this->cur_level = 0;
            this->tree.resize(this->max_level);
        }

        void buildTree(std::vector<L0_node>& nodes);
        void getNonDuplicateNodes();
};