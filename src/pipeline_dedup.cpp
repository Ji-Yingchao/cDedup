#include "general.h"
#include "config.h"
#include "pipeline.h"
#include <vector>
#include <jcr.h>
#include "MetadataManager.h"

static pthread_t dedup_t;
static std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹

static string containers_path;
static unsigned char* container_buf = NULL;
static uint32_t container_index = 0;
static uint32_t container_inner_offset = 0;
static uint16_t container_inner_index = 0;

extern SyncQueue* hash_queue;
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

static void saveFileRecipe(std::vector<std::string> file_recipe, const char* fileRecipesPath){
    int n_version = getFilesNum(fileRecipesPath);
    std::string recipe_name(fileRecipesPath);
    recipe_name.append("/recipe");
    recipe_name.append(std::to_string(n_version));
    int fd = open(recipe_name.data(), O_RDWR | O_CREAT, 0777);
    if(fd < 0){ 
        printf("saveFileRecipe open error, id %d, %s\n", errno, strerror(errno)); 
        exit(-1);
    }
    for(auto x : file_recipe){
        if(write(fd, x.data(), SHA_DIGEST_LENGTH) < 0)
            printf("save recipe write error\n");
    }
    close(fd);
}

static void saveContainerBuf(){
	std::string container_name = containers_path;
    container_name.append("/container");
    container_name.append(std::to_string(container_index));
    int fd = open(container_name.data(), O_RDWR | O_CREAT, 0777);
    if(write(fd, container_buf, container_inner_offset) != container_inner_offset){
        printf("saveContainer write error, id %d, %s\n", errno, strerror(errno));
        exit(-1);
    }
    close(fd);
}

static void resetContainerBuf(){
	memset(container_buf, 0, CONTAINER_SIZE);
	container_index++;
	container_inner_offset = 0;
	container_inner_index = 0;
}

void *dedup_thread(void *arg) {
	container_buf = (unsigned char*)malloc(CONTAINER_SIZE);
	container_index = getFilesNum(Config::getInstance().getContainersPath().c_str());
	containers_path = Config::getInstance().getContainersPath();

	container_inner_offset = 0;
	container_inner_index = 0;	

	struct ENTRY_VALUE entry_value;
	
    while (1) {
		struct chunk *c = NULL;
		c = (struct chunk *)sync_queue_pop(hash_queue);

		if (CHECK_CHUNK(c, CHUNK_FILE_START)) {
			continue;
		}

		if (CHECK_CHUNK(c, CHUNK_FILE_END))
			break;
		
		if (c == NULL)
			break;

		// Insert fingerprint into file recipe
        file_recipe.push_back(std::string((char*)&c->fp, sizeof(fingerprint)));
		jcr.chunk_num += 1;
		jcr.data_size += c->size;

		// lookup fingerprint
        SHA1FP sha1_fp;
		memcpy(&sha1_fp, c->fp, 20);
        LookupResult lookup_result = GlobalMetadataManagerPtr->dedupLookup(&sha1_fp);

		if(lookup_result == Unique){
			jcr.unique_chunk_num += 1;
			jcr.unique_data_size += c->size;

			if(container_inner_offset + c->size >= CONTAINER_SIZE){
				// flush container
				saveContainerBuf();
				resetContainerBuf();
			}

			// append chunk to container buffer
			memcpy(container_buf + container_inner_offset, c->data, c->size);

			// save FP index
			entry_value.container_number = container_index;
			entry_value.offset = container_inner_offset;
			entry_value.chunk_length = c->size;
			entry_value.container_inner_index = container_inner_index;
			GlobalMetadataManagerPtr->addNewEntry(&sha1_fp, &entry_value);

			// container buf pointer
			container_inner_offset += c->size;
			container_inner_index ++;
			
		}else if(lookup_result == Dedup){
			//GlobalMetadataManagerPtr->addRefCnt(sha1_fp);
			;
		}else{
			;
		}
    }

	if(container_inner_offset > 0)
            saveContainerBuf();

	// flush file_recipe
    saveFileRecipe(file_recipe, Config::getInstance().getFileRecipesPath().c_str());

    // save metadata entry
    GlobalMetadataManagerPtr->save();
    
	/* All files done */
    jcr.status = JCR_STATUS_DONE;
    return NULL;
}

void start_dedup_phase() {
	pthread_create(&dedup_t, NULL, dedup_thread, NULL);
}

void stop_dedup_phase() {
	pthread_join(dedup_t, NULL);
    printf("Dedup Phase Over\n");
}