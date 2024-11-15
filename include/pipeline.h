#ifndef PIPELINE_H_
#define PIPELINE_H_

#include "sync_queue.h"

/*
 * CHUNK_FILE_START NORMAL_CHUNK... CHUNK_FILE_END
 */
void start_read_phase();
void stop_read_phase();

/*
 * Input: Raw data blocks
 * Output: Chunks
 */
void start_chunk_phase();
void stop_chunk_phase();

/* Input: Chunks
 * Output: Hashed Chunks.
 */
void start_hash_phase();
void stop_hash_phase();

/*
 * write unique chunks to container
 */
void start_dedup_phase();
void stop_dedup_phase();

void do_restore();

#endif 