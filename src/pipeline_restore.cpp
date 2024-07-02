#include <pthread.h>
#include "pipeline.h"
#include "jcr.h"
#include "string.h"
#include "general.h"
#include "config.h"
#include "MetadataManager.h"
#include "ContainerCache.h"
#include "ChunkCache.h"

extern SyncQueue* restore_recipe_queue;
extern SyncQueue* restore_chunk_queue;
extern MetadataManager *GlobalMetadataManagerPtr;

static uint32_t getFilesNum(const char* dirPath){
    int ans = 0;
    DIR *dir = opendir(dirPath);
    if(!dir){
        printf("getFilesNum opendir error, id %d, %s, the dir is %s\n", 
        errno, strerror(errno), dirPath);
        closedir(dir);
        exit(-1);
    }
    struct dirent* ptr;
    while(readdir(dir)) ans++;
    closedir(dir);
    return ans-2;
}

static std::string getRecipeNameFromVersion(uint8_t restore_version, const char* file_recipe_path){
    std::string recipe_name(file_recipe_path);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(restore_version));
    return recipe_name;
}

static bool fileRecipeExist(uint8_t restore_version, const char* file_recipe_path){
    if((access(getRecipeNameFromVersion(restore_version, file_recipe_path).data(), F_OK)) != -1)    
        return true;    
 
    return false;
}

static void flushAssemblingBuffer(int fd, unsigned char* buf, int len){
    if(write(fd, buf, len) != len){
        printf("Restore, write file error!!!\n");
        exit(-1);
    }
}

static void* fifo_restore_thread(void *arg) {
	RESTORE_METHOD rm = Config::getInstance().getRestoreMethod();
	Cache* cc;
	if(rm == CONTAINER_CACHE){
		cc = new ContainerCache(Config::getInstance().getContainersPath().c_str(), Config::getInstance().getCacheSize());
	}else if(rm == CHUNK_CACHE){
		cc = new ChunkCache(Config::getInstance().getContainersPath().c_str(), 16*1024);
	}
	
	SHA1FP fp;
	ENTRY_VALUE ev;
	struct chunk *c = NULL;
	while ((c = (struct chunk *)sync_queue_pop(restore_recipe_queue))){
		memcpy(&fp, c->fp, sizeof(SHA1FP));
		ev = GlobalMetadataManagerPtr->getEntry(fp);
		c->size = ev.chunk_length;
		c->id = ev.container_number;
		std::string ck_data = cc->getChunkData(ev);
		memcpy(c->data, ck_data.data(), sizeof(SHA1FP));
		sync_queue_push(restore_chunk_queue, c);

		jcr.data_size += ev.chunk_length;
		jcr.chunk_num++;
	}
}


static void* read_recipe_thread(void *arg) {
	int restore_version = Config::getInstance().getRestoreVersion();
	string recipe_path = Config::getInstance().getFileRecipesPath();

	int current_version = getFilesNum(recipe_path.c_str());
	if(current_version != 0){
		if(Config::getInstance().isDeltaDedup()){
			if(Config::getInstance().getMinDR() == 0){
				GlobalMetadataManagerPtr->load(restore_version);
			}else{
				GlobalMetadataManagerPtr->loadVersion(restore_version);
			}
		}else{
			GlobalMetadataManagerPtr->load();
		}
	}

    std::string recipe_name = getRecipeNameFromVersion(restore_version, recipe_path.c_str());
    int fd = open(recipe_name.data(), O_RDONLY, 0777);
    if(fd < 0){
        printf("getFileRecipe open error, %s, %s\n", strerror(errno), recipe_name.data());
        exit(-1);
    }

	struct chunk *c = new_chunk(recipe_name.size()+1);
	strcpy((char*)c->data, recipe_name.data());
	SET_CHUNK(c, CHUNK_FILE_START);
	sync_queue_push(restore_recipe_queue, c);
    
	char fp_buf[SHA_DIGEST_LENGTH]={0};
    while(1){
        int n = read(fd, fp_buf, SHA_DIGEST_LENGTH);
        if(n == 0){
            break;
        }else if(n < 0){
            printf("getFileRecipe error, %s, %s\n", strerror(errno), recipe_name.data());
            exit(-1);
        }else{
			struct chunk* c = new_chunk(0);
			memcpy(&c->fp, fp_buf, SHA_DIGEST_LENGTH);
			sync_queue_push(restore_recipe_queue, c);
        }
    }
    close(fd);

	c = new_chunk(0);
	SET_CHUNK(c, CHUNK_FILE_END);
	sync_queue_push(restore_recipe_queue, c);

	sync_queue_term(restore_recipe_queue);
	return NULL;
}

void* write_restore_data(void* arg) {
	unsigned char* assembling_buffer = (unsigned char*)malloc(FILE_CACHE);

	memset(assembling_buffer, 0, FILE_CACHE);
	int write_buffer_offset = 0;

	int fd = open(Config::getInstance().getRestorePath().c_str(), O_RDWR | O_CREAT, 0777);
	if(fd < 0){
		printf("无法写文件!!! %s\n", strerror(errno));
		exit(-1);
	}

	struct chunk *c = NULL;
	while ((c = (struct chunk *)sync_queue_pop(restore_recipe_queue))){
		if(write_buffer_offset + c->size >= FILE_CACHE){
			flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
			write_buffer_offset = 0;
		}

		memcpy(assembling_buffer + write_buffer_offset, c->data, c->size);

		write_buffer_offset += c->size;
	}
	flushAssemblingBuffer(fd, assembling_buffer, write_buffer_offset);
	close(fd);

    jcr.status = JCR_STATUS_DONE;
    return NULL;
}

void do_restore() {
	restore_chunk_queue = sync_queue_new(100);
	restore_recipe_queue = sync_queue_new(100);

    jcr.status = JCR_STATUS_RUNNING;
	pthread_t recipe_t, read_t, write_t;
	pthread_create(&recipe_t, NULL, read_recipe_thread, NULL);

	if ((Config::getInstance().getRestoreMethod() == CONTAINER_CACHE)||
	(Config::getInstance().getRestoreMethod() == CHUNK_CACHE)) {
		pthread_create(&read_t, NULL, fifo_restore_thread, NULL);
	} else {
		fprintf(stderr, "Invalid restore cache.\n");
		exit(1);
	}

	pthread_create(&write_t, NULL, write_restore_data, NULL);
}

