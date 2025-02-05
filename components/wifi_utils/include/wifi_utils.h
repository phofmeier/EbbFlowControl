#ifndef COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS
#define COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS

#include "esp_err.h"

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
 *
 */
esp_err_t wifi_utils_get_connection_strength(int *rssi_level);

#endif /* COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS */
