#include <esp_err.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "configuration.h"
#include "mqtt5_connection.h"
#include "pump_control.h"
#include "wifi_utils.h"

void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  load_configuration();

  // Create Event Loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize and connect to Wifi
  wifi_utils_init();
  wifi_utils_init_sntp();
  // MQTT Setup
  mqtt5_conn_init();
  // Create control tasks
  create_pump_control_task();
}
