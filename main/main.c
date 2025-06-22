#include "esp_vfs.h"
#include "esp_vfs_fat.h"
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

void initialize_fat_storage() {
  // Handle of the wear levelling library instance
  static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
  const char *base_path = "/store";
  const esp_vfs_fat_mount_config_t mount_config = {
      .max_files = 2,
      .format_if_mount_failed = true,
      .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
      .use_one_fat = false,
  };
  esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage",
                                                   &mount_config, &s_wl_handle);
  if (ret != ESP_OK) {
    ESP_LOGE("FAT", "Failed to mount FAT filesystem: %s", esp_err_to_name(ret));
  } else {
    ESP_LOGI("FAT", "FAT filesystem mounted successfully");
  }
}

void app_main(void) {
  // Configure GPIOS
  configure_pump_output();
  // Initialize storage
  initialize_nvs();
  initialize_fat_storage();

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
