#include "general.h"
#include "config.h"
#include "pipeline.h"
#include <vector>
#include <jcr.h>
#include "MetadataManager.h"
#include <regex>
#include <experimental/filesystem>


static pthread_t dedup_t;
pthread_mutex_t mutex;
static std::vector<std::string> file_recipe; // 保存这个文件所有块的指纹

static string containers_path;
static unsigned char* container_buf = NULL;
static uint32_t container_index = 0;
static uint32_t container_inner_offset = 0;
static uint16_t container_inner_index = 0;

extern SyncQueue* hash_queue;
extern MetadataManager *GlobalMetadataManagerPtr;
namespace fs = std::experimental::filesystem;

bool clear_base_p = true;

// 获取文件版本
static int getVersion(const char* dirPath, const std::string& prefix){
    std::vector<int> recipe_numbers;
    std::regex recipe_pattern(prefix + R"((\d+))");

    // 遍历文件夹中的文件
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        std::string filename = entry.path().filename().string();
        std::smatch match;
        if (std::regex_search(filename, match, recipe_pattern)) {
            int number = std::stoi(match[1].str());
            recipe_numbers.push_back(number);
        }
    }

    // 找到最大的数字
    if (!recipe_numbers.empty()) {
        int last_recipe_number = *std::max_element(recipe_numbers.begin(), recipe_numbers.end());
        return last_recipe_number+1;
    } else {
        return 0;
    }
}

static void saveFileRecipe(std::vector<std::string> file_recipe, const char* fileRecipesPath){
    int n_version = getVersion(fileRecipesPath,"recipe");
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
	container_index = getVersion(Config::getInstance().getContainersPath().c_str(),"container");
	containers_path = Config::getInstance().getContainersPath();

	container_inner_offset = 0;
	container_inner_index = 0;	

	struct ENTRY_VALUE entry_value;
	
	// delta重删
	DEDUP_METHOD dedupMethod = Config::getInstance().getDedupMethod();
    uint32_t current_version = getVersion(Config::getInstance().getFileRecipesPath().c_str(), "recipe");
    FILE_ATTR file_attr = ATTR_BASE;
    if(dedupMethod == DEDUP_INTERVAL){
        uint32_t base_size = Config::getInstance().getBaseSize();
        uint32_t delta_num = Config::getInstance().getDeltaNum();
        file_attr = (current_version % (base_size + delta_num)) > (base_size-1) ? ATTR_DELTA:ATTR_BASE;

        uint32_t min_destination_base = current_version -  current_version % (base_size + delta_num);
        if(current_version == min_destination_base + delta_num)
            GlobalMetadataManagerPtr->clear_base();
    }
    else if(dedupMethod == DEDUP_AUTOMATIC && current_version != 0){    //版本0,初始值满足动态要求
        uint32_t min_dr = Config::getInstance().getMinDR(); 
        auto [attr, dr] = loadDedupRatioAtLine(current_version-1);
        bool clear_base = dr < (double)min_dr/100 && attr == ATTR_DELTA;
        if(clear_base)
            GlobalMetadataManagerPtr->clear_base();

        //清除base的fp后，下一个必是base
        file_attr = clear_base ? ATTR_BASE : ATTR_DELTA;
    }
    else if(dedupMethod == DEDUP_MANUAL){
        vector<FILE_ATTR> attrs = loadDeltaAttrs();
        file_attr = attrs.at(current_version);

        // TODO: 如果有多个small base，也需要清除之前的sbase，但是base和sbase混用
        if(current_version+1 < attrs.size() && attrs.at(current_version+1) == ATTR_BASE)
            GlobalMetadataManagerPtr->clear_base();
    }

    
    // load metadata
    if(current_version != 0){
        if(dedupMethod == DEDUP_GLOBAL){
            GlobalMetadataManagerPtr->load();
        }
        else if(file_attr != ATTR_BASE){
            GlobalMetadataManagerPtr->loadVersion(current_version-1,false);
        }
    }

    while (1) {
		struct chunk *c = (struct chunk *)sync_queue_pop(hash_queue);

        if (c == NULL)
			break;

		if (CHECK_CHUNK(c, CHUNK_FILE_START)) {
            free_chunk(c);
			continue;
		}

		if (CHECK_CHUNK(c, CHUNK_FILE_END)){
            free_chunk(c);
            break;
        }
        
        TIMER_DECLARE(1);
        TIMER_BEGIN(1);
        pthread_mutex_lock(&mutex);

		// Insert fingerprint into file recipe
        file_recipe.push_back(std::string((char*)&c->fp, sizeof(fingerprint)));
		jcr.chunk_num += 1;
		jcr.data_size += c->size;

		// lookup fingerprint
		SHA1FP sha1_fp;
		memcpy(&sha1_fp, c->fp, 20);

		LookupResult lookup_result;
		if(dedupMethod == DEDUP_GLOBAL)
			lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp);
		else
			lookup_result = GlobalMetadataManagerPtr->dedupLookup(sha1_fp, file_attr); 
        
		TIMER_END(1, jcr.dedup_time);

		if(lookup_result == Unique){
			jcr.unique_chunk_num += 1;
			jcr.unique_data_size += c->size;

			if(container_inner_offset + c->size >= CONTAINER_SIZE){
				// flush container
				TIMER_DECLARE(1);
		        TIMER_BEGIN(1);
				saveContainerBuf();
				TIMER_END(1, jcr.write_time);

				resetContainerBuf();
			}

			// append chunk to container buffer
			memcpy(container_buf + container_inner_offset, c->data, c->size);

			// save FP index
			entry_value.container_number = container_index;
			entry_value.offset = container_inner_offset;
			entry_value.chunk_length = c->size;
			entry_value.container_inner_index = container_inner_index;
			entry_value.ref_cnt = 1;
			if(dedupMethod == DEDUP_GLOBAL){
				GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value);
			}else{
				GlobalMetadataManagerPtr->addNewEntry(sha1_fp, entry_value, file_attr);
				
				// TODO: log container sequence
			}

			// container buf pointer
			container_inner_offset += c->size;
			container_inner_index ++;

			
		}else if(lookup_result == Dedup){
			if(dedupMethod == DEDUP_GLOBAL){
				GlobalMetadataManagerPtr->addRefCnt(sha1_fp);
			}else{
				entry_value = GlobalMetadataManagerPtr->addRefCntgetEntry(sha1_fp, file_attr);
				
				// TODO：log container sequence 
			}
		}

        free_chunk(c);
        pthread_mutex_unlock(&mutex);
    }

	if(container_inner_offset > 0)
        saveContainerBuf();

	// flush file_recipe
    saveFileRecipe(file_recipe, Config::getInstance().getFileRecipesPath().c_str());
	double cur_dr = double(jcr.data_size-jcr.unique_data_size) / double(jcr.data_size);

    // save metadata entry
	if(dedupMethod == DEDUP_GLOBAL){
        GlobalMetadataManagerPtr->save();
    }else{
        GlobalMetadataManagerPtr->saveVersion(current_version, file_attr);
        // save dedup ratio and container index sequence
        saveDedupRatio(file_attr,cur_dr);
    }
    
	/* All files done */
    pthread_mutex_lock(&jcr_status_mutex);
    jcr.status = JCR_STATUS_DONE;
    pthread_mutex_unlock(&jcr_status_mutex);
    return NULL;
}

void start_dedup_phase() {
    printf("Dedup Phase Start\n");
    pthread_mutex_init(&mutex, NULL);
	pthread_create(&dedup_t, NULL, dedup_thread, NULL);
}

void stop_dedup_phase() {
	pthread_join(dedup_t, NULL);
    printf("Dedup Phase Over\n");
}