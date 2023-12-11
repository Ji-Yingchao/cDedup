#include "general.h"
#include "config.h"
#include "pipeline.h"

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

        SHA_CTX ctx;
		SHA1_Init(&ctx);
		SHA1_Update(&ctx, c->data, c->size);
		SHA1_Final(c->fp, &ctx);

        sync_queue_push(hash_queue, c);
    }
}

void start_hash_phase() {
	hash_queue = sync_queue_new(200);
	pthread_create(&hash_t, NULL, sha1_thread, NULL);
}

void stop_hash_phase() {
	pthread_join(hash_t, NULL);
    printf("Hash Phase Over\n");
}
