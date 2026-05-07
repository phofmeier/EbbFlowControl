/***
 * Driver for HC-SR04 ultrasonic distance sensor
 */

#ifndef COMPONENTS_DRIVER_HC_SR04_INCLUDE_DRIVER_HC_SR04
#define COMPONENTS_DRIVER_HC_SR04_INCLUDE_DRIVER_HC_SR04

#include "esp_err.h"

void hc_sr04_init();

/****
 * @brief Get distance measurement from the HC-SR04 sensor.
 *
 * This function triggers a measurement and waits for the echo signal to
 * determine the distance. The distance is returned in centimeters.
 *
 * @return Distance in centimeters, or -1 if an error occurred (e.g., timeout).
 */
esp_err_t hc_sr04_get_distance_cm(float *distance_cm);

#endif /* COMPONENTS_DRIVER_HC_SR04_INCLUDE_DRIVER_HC_SR04 */
