#ifndef COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION
#define COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION

#include "cJSON.h"
#include "sdkconfig.h"
#include <stddef.h>

/**
 * @brief Configuration struct for the pump cycles.
 *
 */
struct pump_cycle_configuration_t {
  unsigned short pump_time_s;    // seconds the pump is on.
  unsigned short nr_pump_cycles; // number of pump cycles per day
  unsigned short times_minutes_per_day
      [CONFIG_MAX_NUMBER_PUMP_CYCLES_PER_DAY]; // time in minutes of the day to
                                               // start the pump
};

/**
 * @brief Define the global configuration of the application.
 *
 */
struct configuration_t {
  unsigned char id;
  struct pump_cycle_configuration_t pump_cycles;
};

/**
 * @brief Declaration of the global configuration
 *
 */
extern struct configuration_t configuration;

/**
 * @brief Load the configuration from the persistent storage if available.
 *
 */
void load_configuration();

/**
 * @brief Save the current configuration to the persistent storage.
 *
 */
void save_configuration();

/**
 * @brief Set the configuration from a json string.
 *
 * All keys which are present in the json object are used to override the
 * current config. If a config value has no corresponding key in the json file
 * the old value is not changed.
 *
 * @param json pointer to the json string
 * @param json_length length of the json string
 */
void set_config_from_json(const char *json, size_t json_length);

/**
 * @brief Get the current config as json object.
 *
 * The json object is owned by the caller and needs to be deleted afterwards.
 *
 * @return cJSON* Pointer to a new json object
 */
cJSON *get_config_as_json();

#endif /* COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION */
