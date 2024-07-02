#ifndef GENERAL_H
#define GENERAL_H

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <inttypes.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <openssl/sha.h>

#define MB (1024*1024)
#define FILE_CACHE (256*1024*1024)
#define CONTAINER_SIZE (1*512*1024)


/* the buffer size for read phase */
#define DEFAULT_BLOCK_SIZE (1024*1024*1)

/* chunk */
/* states of normal chunks. */
typedef unsigned char fingerprint[20];
typedef int64_t containerid; //container id
struct chunk {
	int32_t size;
	int flag;
	containerid id;
	fingerprint fp;
	unsigned char *data;
};

#define CHUNK_UNIQUE (0x0000)
#define CHUNK_DUPLICATE (0x0100)
#define TEMPORARY_ID (-1L)
#define CHUNK_FILE_START (0x0001)
#define CHUNK_FILE_END (0x0002)
#define SET_CHUNK(c, f) (c->flag |= f)
#define UNSET_CHUNK(c, f) (c->flag &= ~f)
#define CHECK_CHUNK(c, f) (c->flag & f)

struct chunk* new_chunk(int32_t);
void free_chunk(struct chunk*);
#endif