#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING
/**
 * @brief Data logging interface
 *
 * This interface provides functions to log data to a storage on flash.
 * If an MQTT connection is available, data is sent directly.
 * If the connection is lost, data is stored and sent when the connection is
 * reestablished.
 *
 */

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Enum for data logging event types.
 *
 */
enum data_logging_event_type {
  DATA_LOGGING_EVENT_NEW_DATA = 0,
  DATA_LOGGING_EVENT_CONNECTED = 1,
  DATA_LOGGING_EVENT_DISCONNECTED = 2,
  DATA_LOGGING_EVENT_DATA_PUBLISHED = 3,
};

/**
 * @brief Struct for data logging events.
 */
struct data_logging_event_t {
  enum data_logging_event_type type;
  int id;
};

/**
 * @brief Log the free heap size for debugging.
 *
 */
void log_heap_size();

/**
 * @brief Add new data to the data logging queue.
 *
 * @param topic Topic to which the data is going to be added.
 * @param data Data to be added in cJSON format.
 */
void add_pump_data_item(bool pump_on);
/**
 * @brief Set a connected event.
 *
 */
void set_connected();

/**
 * @brief Set a disconnected event.
 *
 */
void set_disconnected();

/**
 * @brief Set the data published event.
 *
 * @param id message id of the published data.
 */
void set_data_published(unsigned int id);

/**
 * @brief Create a data logging task.
 *
 * @return TaskHandle_t
 */
TaskHandle_t create_data_logging_task();

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING */
