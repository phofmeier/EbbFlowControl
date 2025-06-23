#include "data_logging.h"

#include "configuration.h"
#include "mqtt5_connection.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "freertos/queue.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define TIMEOUT_SENT_DATA 1 * 60 * 1000 / portTICK_PERIOD_MS
#define TIMEOUT_DISCONNECTED 2 * 60 * 60 * 1000 / portTICK_PERIOD_MS
#define FORBIDDEN_PATH_CHAR '='
static const char *TAG = "data_logging";

/* Dimensions of the buffer that the task being created will use as its stack.
   NOTE: This is the number of words the stack will hold, not the number of
   bytes. For example, if each stack item is 32-bits, and this is set to 100,
   then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 8096

/* Structure that will hold the TCB of the task being created. */
static StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
static StackType_t xStack[STACK_SIZE];

/**
 * @brief Path to the data directory where all data files will be stored.
 */
static const char *data_dir_path = "/store/log_data";

/**
 * @brief Event queue for sending and receiving events for the data logging
 * task.
 */
static StaticQueue_t event_queue_;
/**
 * @brief Handle to the event queue used for communication between tasks.
 */
static QueueHandle_t event_queue_handle_;
/**
 * @brief Length of the queue.
 *
 */
#define QUEUE_LENGTH 30
/**
 * @brief Size of one queue item in bytes.
 *
 */
#define EVENT_QUEUE_ITEM_SIZE sizeof(struct data_logging_event_t)

/**
 * @brief Storage area for the event queue.
 *
 */
static uint8_t event_queue_storage_area[QUEUE_LENGTH * EVENT_QUEUE_ITEM_SIZE];

static char current_file_path_in_queue_[256] = "";
static int current_data_id_ = -1;

static unsigned int next_file_id_ = 0;

// void list_dir(char *path) {
//   DIR *dp;
//   struct dirent *ep;
//   dp = opendir(path);
//   ESP_LOGI(TAG, "List dir %s", path);
//   if (dp != NULL) {
//     while ((ep = readdir(dp)) != NULL)
//       ESP_LOGI(TAG, "Found %i, %s", ep->d_type, ep->d_name);

//     (void)closedir(dp);
//   } else {
//     ESP_LOGI(TAG, "Couldn't open the directory");
//   }
// }

void set_connected() {
  ESP_LOGI(TAG, "Setting connected state");
  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_CONNECTED},
      portMAX_DELAY);
}

void set_disconnected() {
  ESP_LOGI(TAG, "Setting disconnected state");
  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_DISCONNECTED},
      portMAX_DELAY);
}

void set_data_published(unsigned int id) {
  ESP_LOGI(TAG, "Setting data published state");
  xQueueSendToBack(event_queue_handle_,
                   (&(struct data_logging_event_t){
                       .type = DATA_LOGGING_EVENT_DATA_PUBLISHED, .id = id}),
                   portMAX_DELAY);
}

/**
 * @brief Replace all occurrences of a character in a string with another.
 *
 * @param str String to modify.
 * @param orig original character to replace.
 * @param rep character to replace with.
 */
void replace_char(char *str, char orig, char rep) {
  char *ix = str;
  while ((ix = strchr(ix, orig)) != NULL) {
    *ix++ = rep;
  }
}

/**
 * @brief Build the data string to send to the mqtt broker.
 *
 * Add id and timestamp to the json data and return as a string.
 *
 * @param data cJSON data to send.
 * @return char* string with the json data needs to be freed.
 */
char *serialize_timed_data(cJSON *data) {
  // add id
  cJSON_AddNumberToObject(data, "id", configuration.id);
  // add current timestamp
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char time_string[70];
  int time_len = strftime(time_string, sizeof(time_string), "%FT%T", &timeinfo);

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  time_len += snprintf(time_string + time_len, sizeof(time_string) - time_len,
                       ".%06ld", tv_now.tv_usec);
  strftime(time_string + time_len, sizeof(time_string) - time_len, "%z",
           &timeinfo);
  cJSON_AddStringToObject(data, "ts", time_string);

  return cJSON_PrintUnformatted(data);
}

/**
 * @brief Get the next data file path from the data directory.
 *
 * This function searches through the data directory and its subdirectories
 * for the next available data file to send.
 */
void get_next_data_file_path() {
  current_file_path_in_queue_[0] = '\0';
  DIR *root_dir = opendir(data_dir_path);
  if (!root_dir) {
    ESP_LOGE(TAG, "Failed to open data directory: %s", data_dir_path);
    return;
  }
  const struct dirent *dir_entry;
  while ((dir_entry = readdir(root_dir)) != NULL) {
    ESP_LOGI(TAG, "Found first dir %s, %i", dir_entry->d_name,
             dir_entry->d_type);
    if (dir_entry->d_type == DT_DIR) {
      // Check if the entry is a directory
      char sub_dir_path[256];
      snprintf(sub_dir_path, sizeof(sub_dir_path), "%.20s/%.220s",
               data_dir_path, dir_entry->d_name);

      DIR *sub_dir = opendir(sub_dir_path);
      if (!sub_dir) {
        ESP_LOGE(TAG, "Failed to open subdirectory: %s", sub_dir_path);
        continue;
      }
      const struct dirent *sub_dir_entry;
      while ((sub_dir_entry = readdir(sub_dir)) != NULL) {
        ESP_LOGI(TAG, "Found sub dir entry %s, %i", sub_dir_entry->d_name,
                 sub_dir_entry->d_type);
        if (sub_dir_entry->d_type == DT_REG) {
          snprintf(current_file_path_in_queue_,
                   sizeof(current_file_path_in_queue_), "%.220s/%.20s",
                   sub_dir_path, sub_dir_entry->d_name);
          closedir(sub_dir);
          closedir(root_dir);
          return;
        }
      }
      closedir(sub_dir);
      int ret = rmdir(sub_dir_path);
      if (ret < 0) {
        ESP_LOGE(TAG, "Failed to remove empty dir %s, %i, %s", sub_dir_path,
                 ret, strerror(errno));
      }
      ESP_LOGI(TAG, "Removed empty subdir %s", sub_dir_path);
    }
  }
  closedir(root_dir);
}

/**
 * @brief Send the current data file to the MQTT broker.
 *
 */
void send_current_data() {
  current_data_id_ = -1;
  if (strlen(current_file_path_in_queue_) == 0) {
    ESP_LOGE(TAG, "No file path in queue to send");
    return;
  }
  // Open the file to read its content
  int fd = open(current_file_path_in_queue_, O_RDONLY);
  if (fd < 0) {
    ESP_LOGE(TAG, "Failed to open file: %s", current_file_path_in_queue_);
    current_file_path_in_queue_[0] = '\0'; // Clear the path if open fails
    return;
  }
  // Read the file content
  char buffer[2048];
  ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);
  if (bytes_read < 0) {
    ESP_LOGE(TAG, "Failed to read file: %s", current_file_path_in_queue_);
    current_file_path_in_queue_[0] = '\0'; // Clear the path if read fails
    return;
  }
  if (bytes_read == sizeof(buffer) - 1) {
    ESP_LOGE(TAG, "File too large to read: %s", current_file_path_in_queue_);
    current_file_path_in_queue_[0] = '\0'; // Clear the path if read fails
    return;
  }
  buffer[bytes_read] = '\0';
  close(fd);
  char topic[256];
  const char *json_ext = strstr(current_file_path_in_queue_, ".json");
  size_t len =
      (json_ext - (current_file_path_in_queue_ + strlen(data_dir_path) + 1)) -
      6;
  memcpy(topic, current_file_path_in_queue_ + strlen(data_dir_path) + 1, len);
  topic[len] = '\0';
  replace_char(topic, FORBIDDEN_PATH_CHAR, '/');

  current_data_id_ = mqtt_sent_message(topic, buffer);
}

/**
 * @brief Schedule the next data send operation.
 *
 * This function checks if there is already a file in the queue. If not, it
 * retrieves the next data file path and sends the data to the MQTT broker.
 */
void schedule_next_data_send() {
  if (strlen(current_file_path_in_queue_) > 0) {
    ESP_LOGW(TAG, "Current file path is not empty, skipping new data");
    // If there is already a file in the queue, skip adding new data
    return;
  }
  get_next_data_file_path();
  if (strlen(current_file_path_in_queue_) == 0) {
    ESP_LOGE(TAG, "Failed to get next data file path %s",
             current_file_path_in_queue_);
    return;
  }
  send_current_data();
  if (current_data_id_ < 0) {
    ESP_LOGE(TAG, "Failed to send data to MQTT");
    current_file_path_in_queue_[0] = '\0';
  }
  return;
}

/**
 * @brief Remove the currently queued file from storage.
 *
 * Happens after the data was successfully sent to the MQTT broker.
 *
 */
void remove_queued_file_from_storage() {
  if (current_file_path_in_queue_[0] == '\0') {
    ESP_LOGE(TAG, "No file path in queue to remove");
    return;
  }
  // Remove the file from storage
  if (remove(current_file_path_in_queue_) != 0) {
    ESP_LOGE(TAG, "Failed to remove file: %s", current_file_path_in_queue_);
  } else {
    ESP_LOGI(TAG, "File removed successfully: %s", current_file_path_in_queue_);
    current_file_path_in_queue_[0] = '\0'; // Clear the path after removal
    current_data_id_ = -1;                 // Reset the current data ID
  }
}

/**
 * @brief Task to handle data logging events.
 *
 * This task waits for new data logging events and schedules sending the data
 * via mqtt.
 *
 * @param arg
 * @return * void
 */
void data_logging_task(void *arg) {
  TickType_t timeout = TIMEOUT_SENT_DATA;
  struct data_logging_event_t event;
  BaseType_t queue_receive_result;
  while (1) {
    // Wait for new data to be added

    queue_receive_result = xQueueReceive(event_queue_handle_, &event, timeout);

    if (queue_receive_result == pdTRUE) { // Event received
      switch (event.type) {
      case DATA_LOGGING_EVENT_NEW_DATA:
        ESP_LOGI(TAG, "New data event received");
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      case DATA_LOGGING_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected event received");
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      case DATA_LOGGING_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected event received");
        current_file_path_in_queue_[0] = '\0';
        timeout = TIMEOUT_DISCONNECTED;
        continue;
      case DATA_LOGGING_EVENT_DATA_PUBLISHED:
        ESP_LOGI(TAG, "Data published event received");
        if (event.id != current_data_id_) {
          ESP_LOGE(TAG, "Invalid data ID received");
          timeout = TIMEOUT_SENT_DATA;
          continue; // Skip processing if ID is invalid
        }
        remove_queued_file_from_storage();
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      default:
        ESP_LOGW(TAG, "Unknown event type: %d", event.type);
        timeout = TIMEOUT_SENT_DATA;
        continue;
      }
    } else {
      ESP_LOGW(TAG, "Event queue timeout");
      // Somehow we run into a timeout during sending data. This should not
      // happen. Just reset and try again.
      current_file_path_in_queue_[0] = '\0';
      schedule_next_data_send();
      if (current_file_path_in_queue_[0] == '\0') {
        timeout = TIMEOUT_DISCONNECTED;
      } else {
        timeout = TIMEOUT_SENT_DATA;
      }
    }
  }
}

unsigned int increment_file_id() {
  next_file_id_++;
  if (next_file_id_ > 99999) {
    next_file_id_ = 0;
  }
  return next_file_id_;
}

void log_heap_size() {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddNumberToObject(data, "free_heap", esp_get_free_heap_size());
  cJSON_AddNumberToObject(data, "min_free_heap",
                          esp_get_minimum_free_heap_size());
  add_timed_data("ef/efc/timed/heap", data);
}

void add_timed_data(const char *topic, cJSON *data) {
  char *json_string = serialize_timed_data(data);
  cJSON_Delete(data);
  char *topic_str = strdup(topic);
  ESP_LOGD(TAG, "Topic before replaced: %s", topic_str);
  replace_char(topic_str, '/', FORBIDDEN_PATH_CHAR);
  char path[256];
  int dir_path_len =
      snprintf(path, sizeof(path), "%s/%s", data_dir_path, topic_str);
  if (dir_path_len < 0 || dir_path_len >= sizeof(path) - 12) {
    ESP_LOGE(TAG, "Path too long: %s", path);
    return;
  }
  // mkdir(path, 0777); // Ensure the directory exists
  int ret = mkdir(path, 0777);
  if (ret < 0) {
    ESP_LOGE(TAG, "Failed to create directory: %s, %i, %s", path, ret,
             strerror(errno));
  }

  snprintf(path + dir_path_len, sizeof(path), "/%05u.json",
           increment_file_id());
  for (size_t i = 0; i < UINT16_MAX && access(path, F_OK) == 0; i++) {
    snprintf(path + dir_path_len, sizeof(path), "/%05u.json",
             increment_file_id());
  }
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0);

  if (fd < 0) {
    ESP_LOGE(TAG, "Failed to open file %s for writing. Error %i", path, fd);
    return;
  }

  ESP_LOGI(TAG, "Writing to the file %s", path);
  write(fd, json_string, strlen(json_string));
  close(fd);
  free(json_string);
  free(topic_str);

  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_NEW_DATA},
      portMAX_DELAY);
}

/**
 * @brief Initialize the data logging task.
 *
 */
void init() {
  event_queue_handle_ =
      xQueueCreateStatic(QUEUE_LENGTH, EVENT_QUEUE_ITEM_SIZE,
                         event_queue_storage_area, &event_queue_);

  // Create data directory if it does not exist
  int ret = mkdir(data_dir_path, 0777);
  if (ret < 0) {
    ESP_LOGE(TAG, "Failed to create a new directory at %s: %i, %s",
             data_dir_path, ret, strerror(errno));
  } else {
    ESP_LOGI(TAG, "Data directory created successfully: %s", data_dir_path);
  }
  // list_dir("/store");
  // list_dir(data_dir_path);
  // list_dir("/store/log_data/ef_efc_timed_pump");
}

/**
 * @brief Create a data logging task object
 *
 * @return TaskHandle_t
 */
TaskHandle_t create_data_logging_task() {
  init(); // Initialize data logging system
  // Static task without dynamic memory allocation
  TaskHandle_t task_handle = xTaskCreateStatic(
      data_logging_task, "DataLogging", /* Task Name */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* No Parameter */
      tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure. */
  return task_handle;
}
