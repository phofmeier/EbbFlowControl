#include "data_logging.h"

#include "configuration.h"
#include "memory_data_store.h"
#include "mqtt5_connection.h"
#include "pump_data_store.h"

#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "freertos/queue.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/** @brief Timeout waiting for sending data via mqtt */
#define TIMEOUT_SENT_DATA 1 * 60 * 1000 / portTICK_PERIOD_MS // 1 minute
/** @brief Timeout waiting after disconnected or if nothing to do. */
#define TIMEOUT_DISCONNECTED 2 * 60 * 60 * 1000 / portTICK_PERIOD_MS // 2 hours

static const char *TAG = "data_logging";

/* Dimensions of the buffer that the task being created will use as its stack.
   NOTE: This is the number of words the stack will hold, not the number of
   bytes. For example, if each stack item is 32-bits, and this is set to 100,
   then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 2000

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

enum data_store_type {
  UNKNOWN,
  MEMORY_DATA_STORE,
  PUMP_DATA_STORE,
};

static enum data_store_type current_data_store_ = UNKNOWN;

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
  ESP_LOGD(TAG, "Setting connected state");
  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_CONNECTED},
      portMAX_DELAY);
}

void set_disconnected() {
  ESP_LOGD(TAG, "Setting disconnected state");
  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_DISCONNECTED},
      portMAX_DELAY);
}

void set_data_published(unsigned int id) {
  ESP_LOGD(TAG, "Setting data published state");
  xQueueSendToBack(event_queue_handle_,
                   (&(struct data_logging_event_t){
                       .type = DATA_LOGGING_EVENT_DATA_PUBLISHED, .id = id}),
                   portMAX_DELAY);
}

bool schedule_next_memory_data_send() {
  static struct memory_data_item_t memory_data_item;
  const bool data_available =
      memory_data_store_pop_and_stash(&memory_data_item);
  if (!data_available) {
    ESP_LOGD(TAG, "No memorydata available to send");
    return false;
  }
  cJSON *data = memory_data_item_to_json(&memory_data_item);
  char *data_json_string = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);
  current_data_id_ = mqtt5_sent_message("ef/efc/timed/heap", data_json_string);
  cJSON_free(data_json_string);

  if (current_data_id_ < 0) {
    ESP_LOGW(TAG, "Failed to send memory data");
    memory_data_store_restore_stack();
    return false;
  }
  return true;
}

bool schedule_next_pump_data_send() {
  static struct pump_data_item_t pump_data_item;
  const bool data_available = pump_data_store_pop_and_stash(&pump_data_item);
  if (!data_available) {
    ESP_LOGD(TAG, "No pump data available to send");
    return false;
  }
  cJSON *data = pump_data_item_to_json(&pump_data_item);
  char *data_json_string = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);
  current_data_id_ =
      mqtt5_sent_message(CONFIG_MQTT_PUMP_STATUS_TOPIC, data_json_string);
  cJSON_free(data_json_string);

  if (current_data_id_ < 0) {
    ESP_LOGW(TAG, "Failed to send pump data");
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
    ESP_LOGD(TAG, "Data already being sent, skipping new data");
    return;
  }
  if (schedule_next_pump_data_send()) {
    current_data_store_ = PUMP_DATA_STORE;
    ESP_LOGD(TAG, "Successfully scheduled next pump data send");
  } else if (schedule_next_memory_data_send()) {
    current_data_store_ = MEMORY_DATA_STORE;
    ESP_LOGD(TAG, "Successfully scheduled next memory data send");
  } else {
    ESP_LOGI(TAG, "Not able to schedule data.");
    current_data_store_ = UNKNOWN;
  }
}

void restore_scheduled_data() {
  if (current_data_id_ >= 0) {
    switch (current_data_store_) {
    case UNKNOWN:
      ESP_LOGD(TAG, "Restoring unknown data store");
      break;
    case PUMP_DATA_STORE:
      pump_data_store_restore_stack();
      ESP_LOGD(TAG, "Restoring pump data store");
      break;
    case MEMORY_DATA_STORE:
      memory_data_store_restore_stack();
      ESP_LOGD(TAG, "Restoring memory data store");
      break;

    default:
      break;
    }
  }
  current_data_id_ = -1;
  current_data_store_ = UNKNOWN;
}

/**
 * @brief Remove the currently queued file from storage.
 *
 * Happens after the data was successfully sent to the MQTT broker.
 *
 */
void remove_queued_file_from_storage() {
  current_data_id_ = -1;
  current_data_store_ = UNKNOWN;
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
  static TickType_t timeout = TIMEOUT_SENT_DATA;
  static struct data_logging_event_t event;
  static BaseType_t queue_receive_result;
  while (1) {
    ESP_LOGD(TAG, "Stack high water mark %d",
             uxTaskGetStackHighWaterMark(NULL));
    // Wait for new data to be added

    queue_receive_result = xQueueReceive(event_queue_handle_, &event, timeout);

    if (queue_receive_result == pdTRUE) { // Event received
      switch (event.type) {
      case DATA_LOGGING_EVENT_NEW_DATA:
        ESP_LOGD(TAG, "New data event received");
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      case DATA_LOGGING_EVENT_CONNECTED:
        ESP_LOGD(TAG, "Connected event received");
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      case DATA_LOGGING_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "Disconnected event received");
        restore_scheduled_data();
        timeout = TIMEOUT_DISCONNECTED;
        continue;
      case DATA_LOGGING_EVENT_DATA_PUBLISHED:
        ESP_LOGD(TAG, "Data published event received");
        if (event.id != current_data_id_) {
          ESP_LOGD(TAG, "Invalid data ID received");
          timeout = TIMEOUT_SENT_DATA;
          continue; // Skip processing if ID is invalid
        }
        remove_queued_file_from_storage();
        schedule_next_data_send();
        timeout = TIMEOUT_SENT_DATA;
        continue;
      default:
        ESP_LOGE(TAG, "Unknown event type: %d", event.type);
        timeout = TIMEOUT_SENT_DATA;
        continue;
      }
    } else {
      ESP_LOGI(TAG, "Event queue timeout");
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

void log_heap_size() {
  size_t out_total_bytes;
  size_t out_used_bytes;
  esp_spiffs_info("storage", &out_total_bytes, &out_used_bytes);

  memory_data_store_push(esp_get_free_heap_size(),
                         esp_get_minimum_free_heap_size(), out_total_bytes,
                         out_used_bytes);

  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_NEW_DATA},
      portMAX_DELAY);
}

/**
 * @brief Initialize the data logging task.
 *
 */
void data_logging_init() {
  event_queue_handle_ =
      xQueueCreateStatic(QUEUE_LENGTH, EVENT_QUEUE_ITEM_SIZE,
                         event_queue_storage_area, &event_queue_);
  pump_data_store_init();
  memory_data_store_init();
}

void add_pump_data_item(bool pump_on) {
  pump_data_store_push(pump_on);
  xQueueSendToBack(
      event_queue_handle_,
      &(struct data_logging_event_t){.type = DATA_LOGGING_EVENT_NEW_DATA},
      portMAX_DELAY);
  log_heap_size();
}

/**
 * @brief Create a data logging task object
 *
 * @return TaskHandle_t
 */
TaskHandle_t create_data_logging_task() {
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
