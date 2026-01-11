#include "esp_event.h"
#include "esp_netif.h"
#include <esp_err.h>

#include "init_utils.c"

#include "config_page.h"
#include "configuration.h"
#include "ota_updater.h"
#include "wifi_utils_sntp.h"
#include "wifi_utils_softap.h"
#include "wifi_utils_sta.h"

void app_main(void) {
  // Initialize storage
  initialize_nvs();

  load_configuration();

  // Create Event Loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize Wifi
  ESP_ERROR_CHECK(esp_netif_init());

  // Start SoftAP and serve configuration page
  wifi_init_softap();
  dhcp_set_captiveportal_url();
  serve_config_page(xTaskGetCurrentTaskHandle());

  if (configuration.network.valid) {
    // if config valid wait for one minute and check if somebody connected to
    // the ap
    const u_int32_t wait_time = pdMS_TO_TICKS(60 * 1e3);
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_time));
  }

  if (configuration.network.valid == false ||
      get_wifi_softap_connections() > 0) {
    // wait indefinitely for configuration
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  }

  // if configured destroy soft ap and start sta
  destroy_softap();

  wifi_utils_init();
  wifi_utils_init_sntp();

  // run ota updater task
  initialize_ota_updater();
  xTaskCreate(&ota_updater_task, "ota_updater_task", 1024 * 8, NULL, 5, NULL);
}
