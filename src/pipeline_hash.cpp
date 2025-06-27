#include "general.h"
#include "config.h"
#include "pipeline.h"
#include "jcr.h"

static pthread_t hash_t;

extern SyncQueue* hash_queue;
extern SyncQueue* chunk_queue;

static void* sha1_thread(void* arg) {
    while (1) {
		struct chunk* c = (struct chunk*)sync_queue_pop(chunk_queue);

		if (c == NULL) {
			sync_queue_term(hash_queue);
			break;
		}

		if (CHECK_CHUNK(c, CHUNK_FILE_START) || CHECK_CHUNK(c, CHUNK_FILE_END)) {
			sync_queue_push(hash_queue, c);
			continue;
		}

		TIMER_DECLARE(1);
		TIMER_BEGIN(1);

        SHA_CTX ctx;
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, c->data, c->size);
		SHA1_Final(c->fp, &ctx);

		TIMER_END(1, jcr.hash_time);
		
        sync_queue_push(hash_queue, c);
    }
	return NULL;
}

void start_hash_phase() {
	hash_queue = sync_queue_new(200);
	assert(hash_queue != NULL);
    assert(hash_queue->queue != NULL);
	printf("Hash Phase Start\n");
	pthread_create(&hash_t, NULL, sha1_thread, NULL);
}

void stop_hash_phase() {
	pthread_join(hash_t, NULL);
    printf("Hash Phase Over\n");
}
