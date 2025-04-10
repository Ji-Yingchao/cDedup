#ifndef VERSION_GC_H
#define VERSION_GC_H
#include "deltaDedup_stats.h"

void deleteFile(int delete_version, FILE_ATTR file_attr);
void do_delete(int current_version);

#endif // VERSION_GC_H
