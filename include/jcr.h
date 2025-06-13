#ifndef Jcr_H_
#define Jcr_H_

#define JCR_STATUS_INIT 1
#define JCR_STATUS_RUNNING 2
#define JCR_STATUS_DONE 3

#include <cstdint>
#include <pthread.h>

/* job control record */
struct jcr{
	int id;
	/*
	 * The path of backup or restore.
	 */
	char* path;

    int status;

	int64_t data_size;
	int64_t unique_data_size;
	int chunk_num;
	int unique_chunk_num;
	int total_container_num;

	double total_time;
	/*
	 * the time consuming of five dedup phase
	 */
	double read_time;
	double chunk_time;
	double hash_time;
	double dedup_time;
	double write_time;

	/*
	 * the time consuming of five dedup phase
	 */
	double read_recipe_time;
	double read_chunk_time;
	double write_chunk_time;

	int read_container_num;
};

extern struct jcr jcr;

extern pthread_mutex_t jcr_status_mutex;

void init_jcr();
void init_backup_jcr();
void show_backup_jcr();
void show_restore_jcr();

#endif /* Jcr_H_ */