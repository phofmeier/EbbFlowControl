#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_LIGHT_DATA_STORE
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_LIGHT_DATA_STORE

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <time.h>

struct light_data_item_t {
  time_t timestamp;   // timestamp when the data was collected
  uint16_t intensity; // intensity value (0-65535)
};

/**
 * @brief Initialize the light data store.
 */
void light_data_store_init();

/**
 * @brief Restore the last light data from the stack.
 */
void light_data_store_restore_stack();

/**
 * @brief Push a new light data item onto the store.
 *
 * @param intensity intensity value to store
 */
void light_data_store_push(uint16_t intensity);

/**
 * @brief Pop a light data item from the store and save it on a stash.
 *
 * @param item pointer to the light_data_item_t where the popped item will be
 * stored
 * @return true if an item was successfully popped, false if the store is empty
 */
bool light_data_store_pop_and_stash(struct light_data_item_t *item);

/**
 * @brief Convert a light data item to a JSON object.
 *
 * @param item pointer to the light_data_item_t to convert
 * @return cJSON* JSON object representing the light data item
 */
cJSON *light_data_item_to_json(const struct light_data_item_t *item);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_LIGHT_DATA_STORE */
