#ifndef COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION
#define COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION

struct pump_cycle_configuration_t
{
    unsigned short pump_time_s;               // seconds the pump is on.
    unsigned short nr_pump_cycles;            // number of pump cycles per day
    unsigned short times_minutes_per_day[24]; // time in minutes of the day to start the pump
};

struct configuration_t
{
    uint8_t id;
    struct pump_cycle_configuration_t pump_cycles;
};

static struct configuration_t configuration = {
    .id = 0,
    .pump_cycles = {
        .pump_time_s = 120,
        .nr_pump_cycles = 3,
        .times_minutes_per_day = {6 * 60, 12 * 60, 20 * 60},
    },
};

void load_configuration();
void save_configuration();

#endif /* COMPONENTS_CONFIGURATION_INCLUDE_CONFIGURATION */
