#ifndef COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION
#define COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION

#define CONFIG_MAX_NUMBER_PUMP_CYCLES_PER_DAY 24

#include <stddef.h>

struct pump_cycle_configuration_t {
  unsigned short pump_time_s;    // seconds the pump is on.
  unsigned short nr_pump_cycles; // number of pump cycles per day
  unsigned short times_minutes_per_day
      [CONFIG_MAX_NUMBER_PUMP_CYCLES_PER_DAY]; // time in minutes of the day to
                                               // start the pump
};

struct configuration_t {
  unsigned char id;
  struct pump_cycle_configuration_t pump_cycles;
};

static struct configuration_t configuration = {
    .id = 0,
    .pump_cycles =
        {
            .pump_time_s = 120,
            .nr_pump_cycles = 3,
            .times_minutes_per_day = {6 * 60, 12 * 60, 20 * 60},
        },
};

void load_configuration();
void save_configuration();

void set_config_from_json(const char *json, size_t json_length);

#endif /* COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION */
