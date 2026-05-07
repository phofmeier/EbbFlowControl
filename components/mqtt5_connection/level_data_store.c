#include "level_data_store.h"
#include "configuration.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

const static char *TAG = "level_data_store";

static SemaphoreHandle_t level_data_items_mutex = NULL;
static StaticSemaphore_t level_data_items_mutex_buffer;

#define MAX_LEVEL_DATA_ITEMS                                                   \
  (CONFIG_MQTT_DATA_LOGGING_LIGHT_STORE_SIZE_MULTIPLE *                        \
   (CONFIG_SPIFFS_PAGE_SIZE / sizeof(struct level_data_item_t)))

#define MAX_FILE_ID 9999
static struct level_data_item_t level_data_items[MAX_LEVEL_DATA_ITEMS];
static unsigned int level_data_items_count = 0;
static struct level_data_item_t level_data_stack_item_;
static bool is_stack_restored = false;

static const char *data_dir_path_ = "/store/log_data/level";

static unsigned int next_file_id_ = 0;

static inline unsigned int increment_file_id() {
  next_file_id_++;
  if (next_file_id_ > MAX_FILE_ID) {
    next_file_id_ = 0;
  }
  return next_file_id_;
}

static char path_[CONFIG_SPIFFS_OBJ_NAME_LEN];

void level_data_store_write_to_disc_() {
  snprintf(path_, sizeof(path_), "%s/%04u.bin", data_dir_path_,
           increment_file_id());
  for (size_t i = 0; i < MAX_FILE_ID && access(path_, F_OK) == 0; i++) {
    snprintf(path_, sizeof(path_), "%s/%04u.bin", data_dir_path_,
             increment_file_id());
  }
  int fd = open(path_, O_RDWR | O_CREAT | O_TRUNC, 0);
  int written_bytes =
      write(fd, (const char *)level_data_items, sizeof(level_data_items));
  close(fd);
  if (written_bytes < 0) {
    ESP_LOGE(TAG, "Failed to write to file %s, error: %s", path_,
             strerror(errno));
    return;
  }
  if (written_bytes != sizeof(level_data_items)) {
    ESP_LOGE(TAG, "Failed to write all data to file %s, written: %d", path_,
             written_bytes);
    return;
  }
  ESP_LOGI(TAG, "Level data written to file %s", path_);
  level_data_items_count = 0;
}

void level_data_store_read_from_disc_() {
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
    ESP_LOGD(TAG, "Reading level data from file: %s", path_);
    int fd = open(path_, O_RDONLY);
    if (fd >= 0) {
      int bytes_read =
          read(fd, (char *)level_data_items, sizeof(level_data_items));
      close(fd);
      if (bytes_read == sizeof(level_data_items)) {
        level_data_items_count = MAX_LEVEL_DATA_ITEMS;
        remove(path_);
        closedir(root_dir);
        ESP_LOGD(TAG, "Level data read from file %s", path_);
        return;
      }
    }
  }
  closedir(root_dir);
}

void level_data_store_init() {
  level_data_items_mutex =
      xSemaphoreCreateMutexStatic(&level_data_items_mutex_buffer);
  level_data_items_count = 0;
  is_stack_restored = false;
  mkdir(data_dir_path_, 0777);
  xSemaphoreGive(level_data_items_mutex);
  ESP_LOGD(TAG, "Level data store initialized with %d items",
           MAX_LEVEL_DATA_ITEMS);
}

void level_data_store_restore_stack() {
  if (xSemaphoreTake(level_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    is_stack_restored = true;
    xSemaphoreGive(level_data_items_mutex);
  }
}

void level_data_store_push(uint32_t distance_mm) {
  if (xSemaphoreTake(level_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    time_t current_time;
    time(&current_time);

    if (level_data_items_count >= MAX_LEVEL_DATA_ITEMS) {
      level_data_store_write_to_disc_();
    }
    level_data_items[level_data_items_count].timestamp = current_time;
    level_data_items[level_data_items_count].distance_mm = distance_mm;
    level_data_items_count++;

    xSemaphoreGive(level_data_items_mutex);
  }
}

bool level_data_store_pop_and_stash(struct level_data_item_t *item) {
  if (xSemaphoreTake(level_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    if (is_stack_restored) {
      is_stack_restored = false;
      item->timestamp = level_data_stack_item_.timestamp;
      item->distance_mm = level_data_stack_item_.distance_mm;
      xSemaphoreGive(level_data_items_mutex);
      return true;
    }

    if (level_data_items_count == 0) {
      level_data_store_read_from_disc_();
    }
    if (level_data_items_count > 0) {
      *item = level_data_items[level_data_items_count - 1];
      level_data_stack_item_ = level_data_items[level_data_items_count - 1];
      level_data_items_count--;
      xSemaphoreGive(level_data_items_mutex);
      return true;
    }
    xSemaphoreGive(level_data_items_mutex);
    return false;
  }
  return false;
}

cJSON *level_data_item_to_json(const struct level_data_item_t *item) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "id", configuration.id);

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
  cJSON_AddNumberToObject(data, "distance_mm", item->distance_mm);
  return data;
}
