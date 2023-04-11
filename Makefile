CC = gcc
LIB = -lcrypto -lz -lstdc++
SRC = main.cpp ./src/fastcdc.cpp ./src/full_file_deduplicater.cpp ./src/merkle_tree.cpp
EXE_NAME = cDedup

amazing:
# 	$(CC)  $(SRC) $(LIB) -o $(EXE_NAME) -pg -I ./include
	$(CC)  $(SRC) $(LIB) -o $(EXE_NAME) -I ./include

clean:
	rm $(EXE_NAME) \
	rm FFFP.meta fingerprints.meta \
	rm -fr FileRecipes FULL_FILE_STORAGE Containers \
	rm L1.meta L2.meta L3.meta L4.meta L5.meta L6.meta