#include "generic_data_store.h"
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

const static char *TAG = "generic_data_store";

#define MAX_FILE_ID 9999

static inline unsigned int increment_file_id(unsigned int *next_file_id) {
  (*next_file_id)++;
  if (*next_file_id > MAX_FILE_ID) {
    *next_file_id = 0;
  }
  return *next_file_id;
}

void generic_data_store_write_to_disc_(struct generic_data_store_t *store) {
  if (!store || !store->items) {
    ESP_LOGE(TAG, "Cannot write to disk: store is not initialized");
    return;
  }
  snprintf(store->path, sizeof(store->path), "%s/%04u.bin",
           store->data_dir_path, increment_file_id(&store->next_file_id));
  for (size_t i = 0; i < MAX_FILE_ID && access(store->path, F_OK) == 0; i++) {
    snprintf(store->path, sizeof(store->path), "%s/%04u.bin",
             store->data_dir_path, increment_file_id(&store->next_file_id));
  }
  int fd = open(store->path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    ESP_LOGE(TAG, "Failed to open file %s for writing, error: %s", store->path,
             strerror(errno));
    return;
  }
  size_t total_size = store->max_items * store->item_size;
  int written_bytes = write(fd, store->items, total_size);
  close(fd);
  if (written_bytes < 0) {
    ESP_LOGE(TAG, "Failed to write to file %s, error: %s", store->path,
             strerror(errno));
    return;
  }
  if ((size_t)written_bytes != total_size) {
    ESP_LOGE(TAG, "Failed to write all data to file %s, written: %d",
             store->path, written_bytes);
    return;
  }
  ESP_LOGI(TAG, "Data written to file %s", store->path);
}

void generic_data_store_read_from_disc_(struct generic_data_store_t *store) {
  if (!store || !store->items) {
    ESP_LOGE(TAG, "Cannot read from disk: store is not initialized");
    return;
  }
  DIR *root_dir = opendir(store->data_dir_path);
  if (!root_dir) {
    ESP_LOGE(TAG, "Failed to open directory: %s", store->data_dir_path);
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
    snprintf(store->path, sizeof(store->path), "%s/%.8s", store->data_dir_path,
             dir_entry->d_name);
    ESP_LOGD(TAG, "Reading data from file: %s", store->path);
    int fd = open(store->path, O_RDONLY);
    if (fd < 0) {
      ESP_LOGE(TAG, "Failed to open file %s for reading, error: %s",
               store->path, strerror(errno));
      continue;
    }
    size_t total_size = store->max_items * store->item_size;
    int bytes_read = read(fd, store->items, total_size);
    close(fd);
    if (bytes_read < 0) {
      ESP_LOGE(TAG, "Failed to read from file %s, error: %s", store->path,
               strerror(errno));
      continue;
    }
    if ((size_t)bytes_read == total_size) {
      store->count = store->max_items;
      remove(store->path);
      closedir(root_dir);
      ESP_LOGD(TAG, "Data read from file %s", store->path);
      return;
    } else {
      ESP_LOGW(TAG, "File %s has incorrect size, expected %zu, got %d",
               store->path, total_size, bytes_read);
      remove(store->path);
    }
  }
  closedir(root_dir);
}

void generic_data_store_init(struct generic_data_store_t *store,
                             size_t item_size, unsigned int max_items,
                             const char *data_dir_path,
                             to_json_func_t to_json_func) {
  store->mutex = xSemaphoreCreateMutexStatic(&store->mutex_buffer);
  store->item_size = item_size;
  store->max_items = max_items;
  store->count = 0;
  store->is_stack_restored = false;
  store->data_dir_path = data_dir_path;
  store->next_file_id = 0;
  store->to_json_func = to_json_func;
  store->items = NULL;
  store->stack_item = NULL;
  store->path[0] = '\0';

  if (item_size == 0 || max_items == 0) {
    ESP_LOGE(TAG, "Invalid generic data store parameters");
    xSemaphoreGive(store->mutex);
    return;
  }

  store->items = malloc(max_items * item_size);
  if (!store->items) {
    ESP_LOGE(TAG, "Failed to allocate data store items (%zu bytes)",
             max_items * item_size);
  }

  store->stack_item = malloc(item_size);
  if (!store->stack_item) {
    ESP_LOGE(TAG, "Failed to allocate data store stack item (%zu bytes)",
             item_size);
    if (store->items) {
      free(store->items);
      store->items = NULL;
    }
  }

  if (data_dir_path) {
    mkdir(data_dir_path, 0777);
  }
  xSemaphoreGive(store->mutex);
  if (store->items && store->stack_item) {
    ESP_LOGD(TAG, "Generic data store initialized with %u items", max_items);
  }
}

void generic_data_store_restore_stack(struct generic_data_store_t *store) {
  if (xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
    if (!store->stack_item) {
      ESP_LOGE(TAG, "Cannot restore stack: no stack buffer allocated");
      store->is_stack_restored = false;
    } else {
      store->is_stack_restored = true;
    }
    xSemaphoreGive(store->mutex);
  }
}

void generic_data_store_push(struct generic_data_store_t *store,
                             const void *item_data) {
  if (xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
    if (!store->items) {
      ESP_LOGE(TAG, "Cannot push data: data store is not initialized");
      xSemaphoreGive(store->mutex);
      return;
    }
    if (store->count >= store->max_items) {
      generic_data_store_write_to_disc_(store);
      store->count = 0;
    }
    memcpy((char *)store->items + store->count * store->item_size, item_data,
           store->item_size);
    store->count++;
    xSemaphoreGive(store->mutex);
  }
}

bool generic_data_store_pop_and_stash(struct generic_data_store_t *store,
                                      void *item) {
  if (xSemaphoreTake(store->mutex, portMAX_DELAY) == pdTRUE) {
    if (!store->items || !store->stack_item) {
      ESP_LOGE(TAG, "Cannot pop data: data store is not initialized");
      xSemaphoreGive(store->mutex);
      return false;
    }
    if (store->is_stack_restored) {
      store->is_stack_restored = false;
      memcpy(item, store->stack_item, store->item_size);
      xSemaphoreGive(store->mutex);
      return true;
    }

    if (store->count == 0) {
      generic_data_store_read_from_disc_(store);
    }
    if (store->count > 0) {
      memcpy(item, (char *)store->items + (store->count - 1) * store->item_size,
             store->item_size);
      memcpy(store->stack_item,
             (char *)store->items + (store->count - 1) * store->item_size,
             store->item_size);
      store->count--;
      xSemaphoreGive(store->mutex);
      return true;
    }
    xSemaphoreGive(store->mutex);
    return false;
  }
  return false;
}
