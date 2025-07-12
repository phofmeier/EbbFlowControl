#include "memory_data_store.h"
#include "configuration.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

const static char *TAG = "memory_data_store";

// Mutex to protect access to the memory data items
static SemaphoreHandle_t memory_data_items_mutex = NULL;
static StaticSemaphore_t memory_data_items_mutex_buffer;

#define MAX_MEMORY_DATA_ITEMS                                                  \
  (CONFIG_MQTT_DATA_LOGGING_MEMORY_STORE_SIZE_MULTIPLE *                       \
   (CONFIG_SPIFFS_PAGE_SIZE / sizeof(struct memory_data_item_t)))
#define MAX_FILE_ID 9999
static struct memory_data_item_t memory_data_items[MAX_MEMORY_DATA_ITEMS];
static unsigned int memory_data_items_count = 0;
static struct memory_data_item_t memory_data_stack_item_;
static bool is_stack_restored = false;

static const char *data_dir_path_ = "/store/log_data/mem";

static unsigned int next_file_id_ =
    0; // File ID for the next file to be created

static inline unsigned int increment_file_id() {
  next_file_id_++;
  if (next_file_id_ > MAX_FILE_ID) {
    next_file_id_ = 0;
  }
  return next_file_id_;
}

/**
 * @brief Static path avoiding allocating mem on the stack
 *
 */
static char path_[CONFIG_SPIFFS_OBJ_NAME_LEN];

/**
 * @brief Write pump data to the disc.
 *
 */
void memory_data_store_write_to_disc_() {
  snprintf(path_, sizeof(path_), "%s/%05u.bin", data_dir_path_,
           increment_file_id());
  for (size_t i = 0; i < MAX_FILE_ID && access(path_, F_OK) == 0; i++) {
    snprintf(path_, sizeof(path_), "%s/%05u.bin", data_dir_path_,
             increment_file_id());
  }
  int fd = open(path_, O_RDWR | O_CREAT | O_TRUNC, 0);
  int written_bytes =
      write(fd, (const char *)memory_data_items, sizeof(memory_data_items));
  close(fd);
  if (written_bytes < 0) {
    ESP_LOGE(TAG, "Failed to write to file %s, error: %s", path_,
             strerror(errno));
    return;
  }
  if (written_bytes != sizeof(memory_data_items)) {
    ESP_LOGE(TAG, "Failed to write all data to file %s, written: %d", path_,
             written_bytes);
    return;
  }
  ESP_LOGI(TAG, "Memory data written to file %s", path_);
  memory_data_items_count = 0;
}

void memory_data_store_read_from_disc_() {
  DIR *root_dir = opendir(data_dir_path_);
  if (!root_dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", data_dir_path_);
    return;
  }
  const struct dirent *dir_entry;
  while ((dir_entry = readdir(root_dir)) != NULL) {
    if (dir_entry->d_type != DT_REG) {
      continue;
    }
    if (strlen(dir_entry->d_name) > 10) {
      continue;
    }

    snprintf(path_, sizeof(path_), "%s/%.8s", data_dir_path_,
             dir_entry->d_name);
    ESP_LOGD(TAG, "Reading memory data from file: %s", path_);
    int fd = open(path_, O_RDONLY);
    if (fd >= 0) {
      int bytes_read =
          read(fd, (char *)memory_data_items, sizeof(memory_data_items));
      close(fd);
      if (bytes_read == sizeof(memory_data_items)) {
        memory_data_items_count = MAX_MEMORY_DATA_ITEMS;
        remove(path_); // Remove the file after loading
        closedir(root_dir);
        ESP_LOGI(TAG, "Memory data read from file %s", path_);
        return;
      }
    }
  }
  closedir(root_dir);
}

void memory_data_store_init() {
  memory_data_items_mutex =
      xSemaphoreCreateMutexStatic(&memory_data_items_mutex_buffer);
  is_stack_restored = false;
  memory_data_items_count = 0;
  mkdir(data_dir_path_, 0777);
  xSemaphoreGive(memory_data_items_mutex);
  ESP_LOGD(TAG, "Memory data store initialized with %d items",
           MAX_MEMORY_DATA_ITEMS);
}

void memory_data_store_restore_stack() {
  if (xSemaphoreTake(memory_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    is_stack_restored = true;
    xSemaphoreGive(memory_data_items_mutex);
  }
}

void memory_data_store_push(const uint32_t free_heap_size,
                            const uint32_t min_free_heap_size,
                            const size_t store_total_bytes,
                            const size_t store_used_bytes) {
  if (xSemaphoreTake(memory_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    time_t current_time;
    time(&current_time);

    if (memory_data_items_count >= MAX_MEMORY_DATA_ITEMS) {
      memory_data_store_write_to_disc_(); // save to disk
    }
    memory_data_items[memory_data_items_count].timestamp = current_time;
    memory_data_items[memory_data_items_count].free_heap_size = free_heap_size;
    memory_data_items[memory_data_items_count].min_free_heap_size =
        min_free_heap_size;
    memory_data_items[memory_data_items_count].store_total_bytes =
        store_total_bytes;
    memory_data_items[memory_data_items_count].store_used_bytes =
        store_used_bytes;
    memory_data_items_count++;

    xSemaphoreGive(memory_data_items_mutex);
  }
}

bool memory_data_store_pop_and_stash(struct memory_data_item_t *item) {
  if (xSemaphoreTake(memory_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    if (is_stack_restored) {
      // return stack
      is_stack_restored = false;
      item->timestamp = memory_data_stack_item_.timestamp;
      item->free_heap_size = memory_data_stack_item_.free_heap_size;
      item->min_free_heap_size = memory_data_stack_item_.min_free_heap_size;
      item->store_total_bytes = memory_data_stack_item_.store_total_bytes;
      item->store_used_bytes = memory_data_stack_item_.store_used_bytes;
      xSemaphoreGive(memory_data_items_mutex);
      return true;
    }

    if (memory_data_items_count == 0) {
      memory_data_store_read_from_disc_();
    }
    if (memory_data_items_count > 0) {
      *item = memory_data_items[memory_data_items_count - 1];
      memory_data_stack_item_ = memory_data_items[memory_data_items_count - 1];
      memory_data_items_count--;
      xSemaphoreGive(memory_data_items_mutex);
      return true; // Successfully popped an item
    }
    xSemaphoreGive(memory_data_items_mutex);
    return false;
  }
  return false;
}

cJSON *memory_data_item_to_json(const struct memory_data_item_t *item) {
  cJSON *data = cJSON_CreateObject();
  // add id
  cJSON_AddNumberToObject(data, "id", configuration.id);
  // add current timestamp
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&item->timestamp, &timeinfo);
  char time_string[32];
  int time_len = strftime(time_string, sizeof(time_string), "%FT%T", &timeinfo);

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  time_len += snprintf(time_string + time_len, sizeof(time_string) - time_len,
                       ".%06ld", tv_now.tv_usec);
  strftime(time_string + time_len, sizeof(time_string) - time_len, "%z",
           &timeinfo);
  cJSON_AddStringToObject(data, "ts", time_string);
  cJSON_AddNumberToObject(data, "free_heap_size", item->free_heap_size);
  cJSON_AddNumberToObject(data, "min_free_heap_size", item->min_free_heap_size);
  cJSON_AddNumberToObject(data, "store_total_bytes", item->store_total_bytes);
  cJSON_AddNumberToObject(data, "store_used_bytes", item->store_used_bytes);

  return data;
}
