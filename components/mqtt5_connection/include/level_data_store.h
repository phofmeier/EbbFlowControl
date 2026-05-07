#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_LEVEL_DATA_STORE
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_LEVEL_DATA_STORE

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct level_data_item_t {
  time_t timestamp;     // timestamp when the data was collected
  uint32_t distance_mm; // level distance in millimeters
};

/**
 * @brief Initialize the level data store.
 */
void level_data_store_init();

/**
 * @brief Restore the last level data from the stack.
 */
void level_data_store_restore_stack();

/**
 * @brief Push a new level data item onto the store.
 *
 * @param distance_mm level distance in millimeters
 */
void level_data_store_push(uint32_t distance_mm);

/**
 * @brief Pop a level data item from the store and save it on a stash.
 *
 * @param item pointer to the level_data_item_t where the popped item will be
 * stored
 * @return true if an item was successfully popped, false if the store is empty
 */
bool level_data_store_pop_and_stash(struct level_data_item_t *item);

/**
 * @brief Convert a level data item to a JSON object.
 *
 * @param item pointer to the level_data_item_t to convert
 * @return cJSON* JSON object representing the level data item
 */
cJSON *level_data_item_to_json(const struct level_data_item_t *item);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_LEVEL_DATA_STORE */
