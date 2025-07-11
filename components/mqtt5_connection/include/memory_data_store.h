#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_MEMORY_DATA_STORE
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_MEMORY_DATA_STORE

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <time.h>

struct memory_data_item_t {
  time_t timestamp;            // timestamp when the data was collected
  uint32_t free_heap_size;     // free heap size at the time of data collection
  uint32_t min_free_heap_size; // minimum free heap size at the time of data
                               // collection
  size_t store_total_bytes;    // total bytes of the store
  size_t store_used_bytes;     // used bytes of the store
};

/**
 * @brief Initialize the pump data store.
 *
 */
void memory_data_store_init();

/**
 * @brief Restore the last memory data from the stack.
 *
 */
void memory_data_store_restore_stack();

/**
 * @brief Push a new memory data item onto the store.
 *
 * @param free_heap_size current free heap size
 * @param min_free_heap_size minimum free heap size
 * @param store_total_bytes total bytes of the store
 * @param store_used_bytes used bytes of the store
 */
void memory_data_store_push(const uint32_t free_heap_size,
                            const uint32_t min_free_heap_size,
                            const size_t store_total_bytes,
                            const size_t store_used_bytes);

/**
 * @brief Pop a memory data item from the store and save it on a stash.
 *
 * @param item pointer to the memory_data_item_t where the popped item will be
 * stored
 * @return true if an item was successfully popped, false if the store is empty
 */
bool memory_data_store_pop_and_stash(struct memory_data_item_t *item);

/**
 * @brief Convert a memory data item to a JSON object.
 *
 * @param item pointer to the memory_data_item_t to convert
 * @return cJSON* JSON object representing the memory data item
 */
cJSON *memory_data_item_to_json(const struct memory_data_item_t *item);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_MEMORY_DATA_STORE */
