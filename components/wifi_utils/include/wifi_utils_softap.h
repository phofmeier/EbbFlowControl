#ifndef COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS_SOFTAP_H
#define COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS_SOFTAP_H

#include "esp_err.h"
#include "esp_event.h"

/**
 * @brief Initialize the WiFi in SoftAP mode.
 */
void wifi_init_softap(void);

/**
 * @brief Set the captive portal URL for DHCP.
 */
void dhcp_set_captiveportal_url(void);

/**
 * @brief Destroy the SoftAP and stop WiFi in AP mode.
 */
void destroy_softap();

/**
 * @brief Get the number of connected stations to the SoftAP.
 *
 * @return int number of connected stations
 */
int get_wifi_softap_connections();

#endif /* COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS_SOFTAP_H */
