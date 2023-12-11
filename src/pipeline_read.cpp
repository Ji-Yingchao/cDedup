#include <pthread.h>
#include "pipeline.h"
#include "jcr.h"
#include "string.h"
#include "general.h"
#include "config.h"

static pthread_t read_t;
extern SyncQueue* read_queue;

static void read_file(char* path) {
	static unsigned char buf[DEFAULT_BLOCK_SIZE];

	FILE *fp;
	if ((fp = fopen(path, "r")) == NULL) {
		perror("The reason is");
		exit(1);
	}

	struct chunk *c = new_chunk(strlen(path) + 1);
	strcpy((char*)c->data, path);

	SET_CHUNK(c, CHUNK_FILE_START);

	sync_queue_push(read_queue, c);

	int size = 0;

	while ((size = fread(buf, 1, DEFAULT_BLOCK_SIZE, fp)) != 0) {
		c = new_chunk(size);
		memcpy(c->data, buf, size);

		sync_queue_push(read_queue, c);
	}

	c = new_chunk(0);
	SET_CHUNK(c, CHUNK_FILE_END);
	sync_queue_push(read_queue, c);

	fclose(fp);
}

static void* read_thread(void *argv) {
	/* Each file will be processed separately */
	read_file((char*)Config::getInstance().getInputPath().c_str());
	sync_queue_term(read_queue);
	return NULL;
}

void start_read_phase() {
    /* running job */
	jcr.status = JCR_STATUS_RUNNING;
	read_queue = sync_queue_new(20);
	pthread_create(&read_t, NULL, read_thread, NULL);
}

void stop_read_phase() {
	pthread_join(read_t, NULL);
	printf("Read Phase Over\n");
}