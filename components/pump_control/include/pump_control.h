#ifndef COMPONENTS_PUMP_CONTROL_INCLUDE_PUMP_CONTROL
#define COMPONENTS_PUMP_CONTROL_INCLUDE_PUMP_CONTROL

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

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
TaskHandle_t create_pump_control_task();

#endif /* COMPONENTS_PUMP_CONTROL_INCLUDE_PUMP_CONTROL */
