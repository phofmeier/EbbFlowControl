#include "data_logging.h"

#include "configuration.h"
#include "mqtt5_connection.h"
#include "pump_data_store.h"

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
#define QUEUE_LENGTH 10
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

static int current_data_id_ = -1;

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

bool schedule_next_pump_data_send() {
  struct pump_data_item_t pump_data_item;
  const bool data_available = pump_data_store_pop_and_stash(&pump_data_item);
  if (!data_available) {
    ESP_LOGW(TAG, "No data available to send");
    return false;
  }
  cJSON *data = pump_data_item_to_json(&pump_data_item);
  char *data_json_string = cJSON_PrintUnformatted(data);
  current_data_id_ =
      mqtt_sent_message(CONFIG_MQTT_PUMP_STATUS_TOPIC, data_json_string);
  cJSON_Delete(data);
  free(data_json_string);
  free(data);
  if (current_data_id_ < 0) {
    ESP_LOGE(TAG, "Failed to send pump data");
    pump_data_store_restore_stack();
    return false;
  }
  return true;
}

/**
 * @brief Schedule the next data send operation.
 *
 * This function checks if there is already a file in the queue. If not, it
 * retrieves the next data file path and sends the data to the MQTT broker.
 */
void schedule_next_data_send() {
  if (current_data_id_ >= 0) {
    ESP_LOGW(TAG, "Data already being sent, skipping new data");
    return;
  }
  if (!schedule_next_pump_data_send()) {
    ESP_LOGE(TAG, "Failed to schedule next data send");
  }
  // Try sending from other store
}

void restore_scheduled_data() {
  if (current_data_id_ >= 0) {
    pump_data_store_restore_stack();
  }
}

/**
 * @brief Remove the currently queued file from storage.
 *
 * Happens after the data was successfully sent to the MQTT broker.
 *
 */
void remove_queued_file_from_storage() { current_data_id_ = -1; }

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
        restore_scheduled_data();
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
      if (current_data_id_ >= 0) {
        restore_scheduled_data();
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      }
      timeout = TIMEOUT_DISCONNECTED;
      continue;
    }
  }
}

// void log_heap_size() {
//   cJSON *data = cJSON_CreateObject();
//   cJSON_AddNumberToObject(data, "free_heap", esp_get_free_heap_size());
//   cJSON_AddNumberToObject(data, "min_free_heap",
//                           esp_get_minimum_free_heap_size());
//   u_int64_t out_total_bytes;
//   u_int64_t out_free_bytes;

//   esp_vfs_fat_info("/store", &out_total_bytes, &out_free_bytes);
//   cJSON_AddNumberToObject(data, "store_total_bytes", out_total_bytes);
//   cJSON_AddNumberToObject(data, "store_free_bytes", out_free_bytes);

//   add_timed_data("ef/efc/timed/heap", data);
// }

/**
 * @brief Initialize the data logging task.
 *
 */
void init() {
  event_queue_handle_ =
      xQueueCreateStatic(QUEUE_LENGTH, EVENT_QUEUE_ITEM_SIZE,
                         event_queue_storage_area, &event_queue_);
  pump_data_store_init();
}

void add_pump_data_item(bool pump_on) {
  pump_data_store_push(pump_on);
  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_NEW_DATA},
      portMAX_DELAY);
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
