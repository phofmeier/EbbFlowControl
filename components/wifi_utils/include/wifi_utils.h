#ifndef COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS
#define COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS

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

#endif /* COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS */
