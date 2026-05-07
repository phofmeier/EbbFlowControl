#ifndef COMPONENTS_MQTT5_CONNECTION_INCLUDE_GENERIC_DATA_STORE
#define COMPONENTS_MQTT5_CONNECTION_INCLUDE_GENERIC_DATA_STORE

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef cJSON *(*to_json_func_t)(const void *item);

struct generic_data_store_t {
  SemaphoreHandle_t mutex;
  StaticSemaphore_t mutex_buffer;
  void *items; // array of items
  size_t item_size;
  unsigned int max_items;
  unsigned int count;
  void *stack_item; // buffer for stack item
  bool is_stack_restored;
  const char *data_dir_path;
  unsigned int next_file_id;
  char path[CONFIG_SPIFFS_OBJ_NAME_LEN];
  to_json_func_t to_json_func;
};

/**
 * @brief Initialize a generic data store.
 *
 * @param store pointer to the generic_data_store_t to initialize
 * @param item_size size of each data item
 * @param max_items maximum number of items
 * @param data_dir_path path to the data directory
 * @param to_json_func function to convert item to JSON
 */
void generic_data_store_init(struct generic_data_store_t *store,
                             size_t item_size, unsigned int max_items,
                             const char *data_dir_path,
                             to_json_func_t to_json_func);

/**
 * @brief Restore the last item from the stack.
 *
 * @param store pointer to the generic_data_store_t
 */
void generic_data_store_restore_stack(struct generic_data_store_t *store);

/**
 * @brief Push a new item onto the store.
 *
 * @param store pointer to the generic_data_store_t
 * @param item_data pointer to the item data to push
 */
void generic_data_store_push(struct generic_data_store_t *store,
                             const void *item_data);

/**
 * @brief Pop an item from the store and save it on a stash.
 *
 * @param store pointer to the generic_data_store_t
 * @param item pointer to where the popped item will be stored
 * @return true if an item was successfully popped, false if the store is empty
 */
bool generic_data_store_pop_and_stash(struct generic_data_store_t *store,
                                      void *item);

#endif /* COMPONENTS_MQTT5_CONNECTION_INCLUDE_GENERIC_DATA_STORE */
