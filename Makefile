CC = gcc
LIB = -lcrypto -lz -lstdc++
SRC = main.cpp ./src/fastcdc.cpp ./src/full_file_deduplicater.cpp
EXE_NAME = cDedup

amazing:
	$(CC)  $(SRC) $(LIB) -o $(EXE_NAME) -g -I ./include

clean:
	rm $(EXE_NAME) \
	rm FFFP.meta fingerprints.meta \
	rm -fr FileRecipes FULL_FILE_STORAGE Containers