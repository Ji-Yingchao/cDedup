#ifndef PIPELINE_H_
#define PIPELINE_H_

#include <sys/time.h>
#include "sync_queue.h"

#define TIMER_DECLARE(n) struct timeval b##n,e##n
#define TIMER_BEGIN(n) gettimeofday(&b##n, NULL)
#define TIMER_END(n,t) gettimeofday(&e##n, NULL); \
    (t)+=e##n.tv_usec-b##n.tv_usec+1000000*(e##n.tv_sec-b##n.tv_sec)

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