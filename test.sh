# FSC 写文件
# ./cDedup --Task write --InputFile /home/cyf/cDedup/test.pdf --ChunkingMethod cdc --Normal 2 --MerkleTree no

# FSC 读文件
# ./cDedup --Task write --InputFile /home/cyf/cDedup/test.pdf --ChunkingMethod cdc --Normal 2 --MerkleTree no

# CDC 写文件
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.16.tar --ChunkingMethod cdc --Normal 2 --MerkleTree no

# CDC 读文件
./cDedup --Task restore --RestorePath /home/cyf/data/res.tar --RestoreVersion 0

# 全文件写

# 全文件读
