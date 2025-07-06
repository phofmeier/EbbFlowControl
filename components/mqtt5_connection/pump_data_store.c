#include "pump_data_store.h"
#include "configuration.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

const static char *TAG = "pump_data_store";

// Mutex to protect access to the pump data items
SemaphoreHandle_t pump_data_items_mutex = NULL;
StaticSemaphore_t pump_data_items_mutex_buffer;

#define MAX_PUMP_DATA_ITEMS 20
#define MAX_PATH_SIZE 31
#define MAX_FILE_ID 99999
static struct pump_data_item_t pump_data_items[MAX_PUMP_DATA_ITEMS];
static unsigned int pump_data_items_count = 0;
static struct pump_data_item_t pump_data_stack_item_;
static bool is_stack_restored = false;

static const char *data_dir_path_ = "/store/log_data/pump";

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
 * @brief Write pump data to the disc.
 *
 */
void pump_data_store_write_to_disc_() {
  char path[MAX_PATH_SIZE];
  snprintf(path, sizeof(path), "%s/%05u.bin", data_dir_path_,
           increment_file_id());
  for (size_t i = 0; i < MAX_FILE_ID && access(path, F_OK) == 0; i++) {
    snprintf(path, sizeof(path), "%s/%05u.bin", data_dir_path_,
             increment_file_id());
  }
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0);
  int written_bytes =
      write(fd, (const char *)pump_data_items, sizeof(pump_data_items));
  close(fd);
  if (written_bytes < 0) {
    ESP_LOGE(TAG, "Failed to write to file %s, error: %s", path,
             strerror(errno));
    return;
  }
  if (written_bytes != sizeof(pump_data_items)) {
    ESP_LOGE(TAG, "Failed to write all data to file %s, written: %d", path,
             written_bytes);
    return;
  }
  ESP_LOGI(TAG, "Pump data written to file %s", path);
  pump_data_items_count = 0;
}

void pump_data_store_read_from_disc_() {
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
    char path[MAX_PATH_SIZE];
    snprintf(path, sizeof(path), "%s/%.9s", data_dir_path_, dir_entry->d_name);
    ESP_LOGI(TAG, "Reading pump data from file: %s", path);
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
      int bytes_read =
          read(fd, (char *)pump_data_items, sizeof(pump_data_items));
      close(fd);
      if (bytes_read == sizeof(pump_data_items)) {
        pump_data_items_count = MAX_PUMP_DATA_ITEMS;
        remove(path); // Remove the file after loading
        closedir(root_dir);
        ESP_LOGI(TAG, "Pump data read from file %s", path);
        return;
      }
    }
  }
  closedir(root_dir);
}

void pump_data_store_init() {
  pump_data_items_mutex =
      xSemaphoreCreateMutexStatic(&pump_data_items_mutex_buffer);
  xSemaphoreGive(pump_data_items_mutex);
  pump_data_items_count = 0;
  mkdir(data_dir_path_, 0777);
}

void pump_data_store_restore_stack() {
  if (xSemaphoreTake(pump_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    is_stack_restored = true;
    xSemaphoreGive(pump_data_items_mutex);
  }
}

void pump_data_store_push(bool pump_on) {
  if (xSemaphoreTake(pump_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    time_t current_time;
    time(&current_time);

    if (pump_data_items_count >= MAX_PUMP_DATA_ITEMS) {
      pump_data_store_write_to_disc_(); // save to disk
    }
    pump_data_items[pump_data_items_count].timestamp = current_time;
    pump_data_items[pump_data_items_count].pump_on = pump_on;
    pump_data_items_count++;

    xSemaphoreGive(pump_data_items_mutex);
  }
}

bool pump_data_store_pop_and_stash(struct pump_data_item_t *item) {
  if (xSemaphoreTake(pump_data_items_mutex, portMAX_DELAY) == pdTRUE) {
    if (is_stack_restored) {
      // return stack
      is_stack_restored = false;
      item->timestamp = pump_data_stack_item_.timestamp;
      item->pump_on = pump_data_stack_item_.pump_on;
      xSemaphoreGive(pump_data_items_mutex);
      return true;
    }

    if (pump_data_items_count == 0) {
      pump_data_store_read_from_disc_();
    }
    if (pump_data_items_count > 0) {
      *item = pump_data_items[pump_data_items_count - 1];
      pump_data_stack_item_ = pump_data_items[pump_data_items_count - 1];
      pump_data_items_count--;
      xSemaphoreGive(pump_data_items_mutex);
      return true; // Successfully popped an item
    }
    xSemaphoreGive(pump_data_items_mutex);
    return false;
  }
  return false;
}

cJSON *pump_data_item_to_json(const struct pump_data_item_t *item) {
  cJSON *data = cJSON_CreateObject();
  // add id
  cJSON_AddNumberToObject(data, "id", configuration.id);
  // add current timestamp
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&item->timestamp, &timeinfo);
  char time_string[70];
  int time_len = strftime(time_string, sizeof(time_string), "%FT%T", &timeinfo);

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  time_len += snprintf(time_string + time_len, sizeof(time_string) - time_len,
                       ".%06ld", tv_now.tv_usec);
  strftime(time_string + time_len, sizeof(time_string) - time_len, "%z",
           &timeinfo);
  cJSON_AddStringToObject(data, "ts", time_string);
  if (item->pump_on) {
    cJSON_AddStringToObject(data, "status", "start");
  } else {
    cJSON_AddStringToObject(data, "status", "stop");
  }

  return data;
}
