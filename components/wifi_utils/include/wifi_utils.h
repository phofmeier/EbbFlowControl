#ifndef COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS
#define COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Initialize the wifi and connect to the defined network.
 *
 */
void wifi_utils_init(void);

/**
 * @brief Initialize the connection to the SNTP Server to synchronize the time.
 *
 */
void wifi_utils_init_sntp(void);

/**
 * @brief Get the wifi connection strength.
 *
 * @param rssi_level output of the rssi_level of the wifi connection
 *
 * @return ESP_OK: succeed - ESP_ERR_INVALID_ARG: invalid argument - ESP_FAIL:
 * failed
 */
esp_err_t wifi_utils_get_connection_strength(int *rssi_level);

/**
 * @brief Connect to the WiFi network.
 *
 */
void wifi_utils_connect();

/**
 * @brief Connect to the wifi network blocking until the connection is
 * established.
 *
 * @return ESP_OK: wifi connected - ESP_FAIL: connection failed
 */
esp_err_t wifi_utils_connect_wifi_blocking();

/**
 * @brief Task to check WiFi connection and retry if it fails.
 *
 * @param pvParameters Task parameters (unused)
 */
void wifi_utils_check_connection_task(void *pvParameters);

/**
 * @brief Create the task to check the wifi connection and retry if it fails.
 *
 * @return TaskHandle_t handle to the created task
 */
TaskHandle_t wifi_utils_create_connection_checker_task();

#endif /* COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS */
