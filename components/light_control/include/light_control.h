/**
 * @file light_control.h
 * @brief Control a grow light using the GP8211S I2C to 0V-10V analog driver.
 */

#ifndef COMPONENTS_LIGHT_CONTROL_INCLUDE_LIGHT_CONTROL
#define COMPONENTS_LIGHT_CONTROL_INCLUDE_LIGHT_CONTROL

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

void initialize_light_control();

TaskHandle_t create_light_control_task();

#endif /* COMPONENTS_LIGHT_CONTROL_INCLUDE_LIGHT_CONTROL */
