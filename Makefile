CC = gcc
LIB = -lcrypto -lz -lstdc++
SRC = main.cpp 
amazing:
	$(CC)  $(SRC) $(LIB) -o cDedup -g 