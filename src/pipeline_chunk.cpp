#include "general.h"
#include "config.h"
#include "pipeline.h"
#include "fastcdc.h"

static pthread_t chunk_t;
static int64_t chunk_num;

static int (*chunking)(unsigned char* buf, int size);
extern SyncQueue* read_queue;
extern SyncQueue* chunk_queue;

static void* chunk_thread(void *arg) {
    int rest_size = 0;
	int buf_off = 0;
    int max_chunk_size = Config::getInstance().getAvgChunkSize() * 4;
	unsigned char *block_buf = (unsigned char*)malloc(DEFAULT_BLOCK_SIZE + max_chunk_size);

    struct chunk* c = NULL;

    while (1) {
        /* Try to receive a CHUNK_FILE_START. */
		c = (struct chunk*)sync_queue_pop(read_queue);

		if (c == NULL) {
			sync_queue_term(chunk_queue);
			break;
		}

        assert(CHECK_CHUNK(c, CHUNK_FILE_START));
		sync_queue_push(chunk_queue, c);

        /* Try to receive normal chunks. */
		c = (struct chunk*)sync_queue_pop(read_queue);
		if (!CHECK_CHUNK(c, CHUNK_FILE_END)) {
			memcpy(block_buf, c->data, c->size);
			rest_size += c->size;
			free_chunk(c);
			c = NULL;
		}

        while (1) {
			/* c == NULL indicates more data for this file can be read. */
			while ((rest_size < max_chunk_size) && c == NULL) {
				c = (struct chunk*)sync_queue_pop(read_queue);
				if (!CHECK_CHUNK(c, CHUNK_FILE_END)) {
					memmove(block_buf, block_buf + buf_off, rest_size);
					buf_off = 0;
					memcpy(block_buf + rest_size, c->data, c->size);
					rest_size += c->size;
					free_chunk(c);
					c = NULL;
				}
			}

            if (rest_size == 0) {
				assert(c);
				break;
			}

            int	chunk_size = chunking(block_buf + buf_off, rest_size);

            struct chunk *nc = new_chunk(chunk_size);
			memcpy(nc->data, block_buf + buf_off, chunk_size);
			rest_size -= chunk_size;
			buf_off += chunk_size;

            sync_queue_push(chunk_queue, nc);
		}

        sync_queue_push(chunk_queue, c);
		buf_off = 0;
		c = NULL;
    }

    free(block_buf);
	return NULL;
}

void chunking_method_prepare(){
    if(Config::getInstance().getChunkingMethod() == FSC){
		if(Config::getInstance().getAvgChunkSize() == 4096)
		    chunking = FSC_4;
        else if(Config::getInstance().getAvgChunkSize() == 8192)
		    chunking = FSC_8;
        else if(Config::getInstance().getAvgChunkSize() == 16384)
		    chunking = FSC_16;
        else{
            printf("invalid average chunk size for FSC: %d\n", Config::getInstance().getAvgChunkSize());
            exit(-1);
        }

	}else if(Config::getInstance().getChunkingMethod() == CDC){
		printf("Deploying FastCDC chunking method\n");
		chunking = FastCDC_with_NC;
        fastCDC_init(Config::getInstance().getAvgChunkSize(), Config::getInstance().getNormalLevel());
		
	}else{
		printf("invalid average chunk size for FSC: %d\n", Config::getInstance().getAvgChunkSize());
        exit(-1);
	}
}

void start_chunk_phase() {
    chunking_method_prepare();
	chunk_queue = sync_queue_new(200);
	pthread_create(&chunk_t, NULL, chunk_thread, NULL);
}

void stop_chunk_phase() {
	pthread_join(chunk_t, NULL);
    printf("Chunk Phase Over\n");
}