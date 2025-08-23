#include "esp_log.h"
// #include "esp_spiffs.h"
// #include "esp_vfs.h"
#include <esp_err.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "configuration.h"
// #include "data_logging.h"
// #include "mqtt5_connection.h"
// #include "pump_control.h"
// #include "wifi_utils.h"

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
  //   ESP_ERROR_CHECK(esp_event_loop_create_default());

  //   // Initialize and connect to Wifi
  //   wifi_utils_init();
  //   wifi_utils_init_sntp();
  //   wifi_utils_create_connection_checker_task();
  //   // MQTT Setup
  //   mqtt5_conn_init();
  //   mqtt5_create_connection_checker_task();
  //   // Data Logging Setup
  //   create_data_logging_task();
  //   // Create control tasks
  //   ESP_ERROR_CHECK(add_notify_for_new_config(create_pump_control_task()));
}
