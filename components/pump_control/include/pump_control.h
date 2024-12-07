#ifndef COMPONENTS_PUMP_CONTROL_INCLUDE_PUMP_CONTROL
#define COMPONENTS_PUMP_CONTROL_INCLUDE_PUMP_CONTROL

/**
 * @brief RTOS Task implementation
 *
 * @param pvParameters unused (NULL)
 */
void pump_control_task(void *pvParameters);

/**
 * @brief Create and start the task for controlling the pump.
 *
 * The pump is activated for a specific amount of seconds. This happens multiple
 * times per day. The time and times can be configured in the global
 * configuration.
 *
 */
void create_pump_control_task();

#endif /* COMPONENTS_PUMP_CONTROL_INCLUDE_PUMP_CONTROL */
