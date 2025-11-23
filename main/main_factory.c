#include "esp_event.h"
#include "esp_log.h"
#include <esp_err.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "configuration.h"
#include "ota_updater.h"
#include "wifi_utils.h"

void initialize_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void app_main(void) {
  // Initialize storage
  initialize_nvs();

  load_configuration();

  // Create Event Loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize and connect to Wifi
  wifi_utils_init();
  wifi_utils_init_sntp();

  // run ota updater task
  initialize_ota_updater();
  xTaskCreate(&ota_updater_task, "ota_updater_task", 1024 * 8, NULL, 5, NULL);
}
