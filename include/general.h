#ifndef GENERAL_H
#define GENERAL_H

#define FILE_CACHE (256*1024*1024)
#define CONTAINER_SIZE (4*1024*1024)

// <SHA1 20B, ContainerNumber 4B, offset 4B, size 2B, ContainerInnerIndex 2B, RefCnt 4B>
#define META_DATA_SIZE 36
#define SHA1_LENGTH 20
#define VALUE_LENGTH 16

#endif