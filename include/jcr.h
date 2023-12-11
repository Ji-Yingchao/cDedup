#ifndef Jcr_H_
#define Jcr_H_

#define JCR_STATUS_INIT 1
#define JCR_STATUS_RUNNING 2
#define JCR_STATUS_DONE 3

/* job control record */
struct jcr{
	int id;
	/*
	 * The path of backup or restore.
	 */
	char* path;

    int status;

	int data_size;
	int unique_data_size;
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
};

extern struct jcr jcr;

void init_jcr();
void init_backup_jcr();
void show_backup_jcr();

#endif /* Jcr_H_ */