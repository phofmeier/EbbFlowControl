#ifndef CONFIG_PAGE_INCLUDE_CONFIG_PAGE
#define CONFIG_PAGE_INCLUDE_CONFIG_PAGE
#include "freertos/FreeRTOS.h"
/**
 * @brief Serve the configuration page by starting the web server
 */
void serve_config_page(TaskHandle_t configuration_task_handle);

#endif /* CONFIG_PAGE_INCLUDE_CONFIG_PAGE */
