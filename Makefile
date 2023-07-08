CC = gcc
LIB = -lcrypto -lz -lstdc++ -lcjson
SRC = main.cpp ./src/fastcdc.cpp ./src/full_file_deduplicater.cpp ./src/merkle_tree.cpp \
	  ./src/MetadataManager.cpp ./src/ContainerCache.cpp ./src/ChunkCache.cpp

EXE_NAME = cDedup

amazing:
	$(CC)  $(SRC) $(LIB) -o $(EXE_NAME) -g -I./include -I./utils

clean:
	rm $(EXE_NAME) \
	rm TEST/metadata/FFFP.meta TEST/metadata/fingerprints.meta \
	rm -fr TEST/metadata/FileRecipes TEST/metadata/FULL_FILE_STORAGE TEST/metadata/Containers \
	rm TEST/metadata/L1.meta TEST/metadata/L2.meta TEST/metadata/L3.meta TEST/metadata/L4.meta TEST/metadata/L5.meta TEST/metadata/L6.meta

