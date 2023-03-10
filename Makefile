CC = gcc
LIB = -lcrypto -lz -lstdc++
SRC = main.cpp ./src/fastcdc.cpp
EXE_NAME = cDedup

amazing:
	$(CC)  $(SRC) $(LIB) -o $(EXE_NAME) -g -I ./include

clean:
	rm $(EXE_NAME)