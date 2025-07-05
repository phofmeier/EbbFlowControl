#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_PUMP_DATA_STORE
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_PUMP_DATA_STORE

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <time.h>

struct pump_data_item_t {
  time_t timestamp; // timestamp when the data was collected
  bool pump_on;     // true if the pump was on, false otherwise
};

/**
 * @brief Initialize the pump data store.
 *
 */
void pump_data_store_init();

/**
 * @brief Restore the last pump data from the stack.
 *
 */
void pump_data_store_restore_stack();

/**
 * @brief Push a new pump data item onto the store.
 *
 * @param pump_on true if the pump is on, false otherwise
 */
void pump_data_store_push(bool pump_on);

/**
 * @brief Pop a pump data item from the store and save it on a stash.
 *
 * @param item pointer to the pump_data_item_t where the popped item will be
 * stored
 * @return true if an item was successfully popped, false if the store is empty
 */
bool pump_data_store_pop_and_stash(struct pump_data_item_t *item);

/**
 * @brief Convert a pump data item to a JSON object.
 *
 * @param item pointer to the pump_data_item_t to convert
 * @return cJSON* JSON object representing the pump data item
 */
cJSON *pump_data_item_to_json(const struct pump_data_item_t *item);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_PUMP_DATA_STORE */
