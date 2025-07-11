#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <esp_err.h>
#include <nvs_flash.h>
#include <stdio.h>

#include "configuration.h"
#include "data_logging.h"
#include "mqtt5_connection.h"
#include "pump_control.h"
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

/**
 * @brief Initialize th SPIFFS filesystem for storing logging data.
 *
 */
void initialize_spiffs_storage() {
  const char *base_path = "/store";
  const esp_vfs_spiffs_conf_t mount_config = {.base_path = base_path,
                                              .partition_label = "storage",
                                              .max_files = 5,
                                              .format_if_mount_failed = true};
  esp_err_t ret = esp_vfs_spiffs_register(&mount_config);

  if (ret != ESP_OK) {
    ESP_LOGE("SPIFFS", "Failed to mount SPIFFS filesystem: %s",
             esp_err_to_name(ret));
  } else {
    ESP_LOGI("SPIFFS", "SPIFFS filesystem mounted successfully");
  }
}

void app_main(void) {
  // Configure GPIOS
  configure_pump_output();
  // Initialize storage
  initialize_nvs();
  initialize_spiffs_storage();

  load_configuration();

  // Create Event Loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize and connect to Wifi
  wifi_utils_init();
  wifi_utils_init_sntp();
  wifi_utils_create_connection_checker_task();
  // MQTT Setup
  mqtt5_conn_init();
  mqtt5_create_connection_checker_task();
  // Data Logging Setup
  create_data_logging_task();
  // Create control tasks
  ESP_ERROR_CHECK(add_notify_for_new_config(create_pump_control_task()));
}
