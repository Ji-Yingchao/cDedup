# FSC 写文件
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.16.tar --ChunkingMethod fsc --MerkleTree no
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.17.tar --ChunkingMethod fsc --MerkleTree no
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.18.tar --ChunkingMethod fsc --MerkleTree no

# CDC 写文件
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.16.tar --ChunkingMethod cdc --Normal 2 --MerkleTree no
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.17.tar --ChunkingMethod cdc --Normal 2 --MerkleTree no
# ./cDedup --Task write --InputFile /home/cyf/data/linux-5.18.tar --ChunkingMethod cdc --Normal 2 --MerkleTree no

# 分块读文件
# ./cDedup --Task restore --RestorePath /home/cyf/data/res0.tar --RestoreVersion 0 --RestoreMethod container
# ./cDedup --Task restore --RestorePath /home/cyf/data/res1.tar --RestoreVersion 1 --RestoreMethod container
# ./cDedup --Task restore --RestorePath /home/cyf/data/res2.tar --RestoreVersion 2 --RestoreMethod container

# 全文件写
# ./cDedup --Task write --InputFile /home/cyf/cDedup/FAST15.pdf --ChunkingMethod file
# ./cDedup --Task write --InputFile /home/cyf/cDedup/FAST15.pdf --ChunkingMethod file
# ./cDedup --Task write --InputFile /home/cyf/cDedup/FAST16.pdf --ChunkingMethod file
# ./cDedup --Task write --InputFile /home/cyf/cDedup/FAST17.pdf --ChunkingMethod file
# ./cDedup --Task write --InputFile /home/cyf/cDedup/FAST20.pdf --ChunkingMethod file

# 全文件读
./cDedup --Task restore --RestorePath /home/cyf/data/file1.pdf --RestoreId 0 --ChunkingMethod file
./cDedup --Task restore --RestorePath /home/cyf/data/file2.pdf --RestoreId 1 --ChunkingMethod file
./cDedup --Task restore --RestorePath /home/cyf/data/file3.pdf --RestoreId 2 --ChunkingMethod file
./cDedup --Task restore --RestorePath /home/cyf/data/file4.pdf --RestoreId 3 --ChunkingMethod file