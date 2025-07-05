
/**
 * interface to add data
 * all data is saved to a storage fat on flash
 * if mqtt connection is available
 * send data directly
 * wait until data was sent successfully before deleting
 * else: just store data
 * if connection gets reestablished
 * send all data which was not sended yet
 *
 * each topic gets own folder
 * each data point gets own file
 * file name is a unique id
 *
 * publish data topic and id to mqtt interface
 * mqtt returns ids of sent data. -> delete data
 *
 * use two queues
 * one data in queue where other tasks can add data
 * on data out queue where data is sent to mqtt
 *
 */

#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_DATA_LOGGING

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum data_logging_event_type {
  DATA_LOGGING_EVENT_NEW_DATA = 0,
  DATA_LOGGING_EVENT_CONNECTED = 1,
  DATA_LOGGING_EVENT_DISCONNECTED = 2,
  DATA_LOGGING_EVENT_DATA_PUBLISHED = 3,
};

struct data_logging_event_t {
  enum data_logging_event_type type;
  int id;
};

/**
 * @brief Log the free heap size for debugging.
 *
 */
// void log_heap_size();

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
