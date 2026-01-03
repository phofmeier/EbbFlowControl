#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include <esp_err.h>
#include <nvs_flash.h>

/**
 * @brief Initialize the nvs storage for configurations.
 *
 */
static inline void initialize_nvs() {
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
static inline void initialize_spiffs_storage() {
  const char *base_path = "/store";
  const esp_vfs_spiffs_conf_t mount_config = {.base_path = base_path,
                                              .partition_label = "storage",
                                              .max_files = 2,
                                              .format_if_mount_failed = true};
  esp_err_t ret = esp_vfs_spiffs_register(&mount_config);

  if (ret != ESP_OK) {
    ESP_LOGE("SPIFFS", "Failed to mount SPIFFS filesystem: %s",
             esp_err_to_name(ret));
  } else {
    ESP_LOGD("SPIFFS", "SPIFFS filesystem mounted successfully");
  }
}
