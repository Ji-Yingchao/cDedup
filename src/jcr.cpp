/*job control record*/

#include "jcr.h"
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "general.h"

struct jcr jcr;

void init_jcr() {
	jcr.status = JCR_STATUS_INIT;

	jcr.data_size = 0;
	jcr.unique_data_size = 0;
	jcr.chunk_num = 0;
	jcr.unique_chunk_num = 0;

	jcr.total_time = 0;
	jcr.read_container_num = 0;
}

void init_backup_jcr() {
	init_jcr();
}

void show_backup_jcr(){
	float throughput = (float)(jcr.data_size) / MB / ((float)(jcr.total_time)/1000000);
	printf("========= backup end =========\n");
	printf("Throughput %.2f MiB/s\n", throughput);
    printf("Dedup Ratio %.2f\n", double(jcr.data_size - jcr.unique_data_size) / double(jcr.data_size) *100);
	printf("chunk_num %d\n", jcr.chunk_num);
	printf("unique_chunk_num %d\n", jcr.unique_chunk_num);
	printf("data_size %d\n", jcr.data_size);
	printf("unique_data_size %d\n", jcr.unique_data_size);
	printf("dedup_data_size %d\n", jcr.data_size - jcr.unique_data_size);
}

void show_restore_jcr(){
	printf("========= restore end =========\n");
	printf("total size(B): %" PRId32 "\n", jcr.data_size);
	printf("number of chunks: %" PRId32"\n", jcr.chunk_num);
	printf("throughput(MB/s): %.2f\n",
		(double) jcr.data_size * 1000000 / (1024 * 1024 * jcr.total_time));
	printf("speed factor: %.2f\n",
			jcr.data_size / (1024.0 * 1024 * jcr.read_container_num));
}