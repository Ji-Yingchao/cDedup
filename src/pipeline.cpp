#include "general.h"
#include "pipeline.h"

/* Output of read phase. */
SyncQueue* read_queue;
/* Output of chunk phase. */
SyncQueue* chunk_queue;
/* Output of hash phase. */
SyncQueue* hash_queue;

/* Output of read recipe phase. */
SyncQueue* restore_recipe_queue;
/* Output of read chunk phase. */
SyncQueue* restore_chunk_queue;

struct chunk* new_chunk(int32_t size) {
	struct chunk* ck = (struct chunk*) malloc(sizeof(struct chunk));

	ck->flag = CHUNK_UNIQUE;
	ck->id = TEMPORARY_ID;
	memset(&ck->fp, 0x0, sizeof(fingerprint));
	ck->size = size;

	if (size > 0)
		ck->data = (unsigned char*)malloc(size);
	else
		ck->data = NULL;

	return ck;
}

void free_chunk(struct chunk* ck) {
	if (ck->data) {
		free(ck->data);
		ck->data = NULL;
	}
	free(ck);
}