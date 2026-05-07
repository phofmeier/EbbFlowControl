/**
 * @file level_sensor.h
 * @brief Level sensor measurement task using HC-SR04 ultrasonic driver.
 */

#ifndef COMPONENTS_LEVEL_SENSOR_INCLUDE_LEVEL_SENSOR
#define COMPONENTS_LEVEL_SENSOR_INCLUDE_LEVEL_SENSOR

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Initialize the level sensor.
 *
 * This function configures the HC-SR04 trigger and echo pins so the
 * level sensor task can perform measurements.
 */
void level_sensor_init(void);

/**
 * @brief Create and start the level sensor measurement task.
 *
 * The task measures the level using the HC-SR04 sensor every 10 minutes and
 * logs the measured distance.
 *
 * @return Handle to the created task, or NULL if creation failed.
 */
TaskHandle_t create_level_sensor_task(void);

#endif /* COMPONENTS_LEVEL_SENSOR_INCLUDE_LEVEL_SENSOR */
