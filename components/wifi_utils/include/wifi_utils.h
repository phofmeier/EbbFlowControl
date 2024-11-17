#ifndef COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS
#define COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS

#include "esp_event.h"

void wifi_utils_init(void);

void wifi_utils_init_sntp(void);

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);



#endif /* COMPONENTS_WIFI_UTILS_INCLUDE_WIFI_UTILS */
