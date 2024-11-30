#include "configuration.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>

#define CONFIG_STORAGE_NAMESPACE "EFCConf"
#define CONFIG_ID_NAME "id"
#define CONFIG_PUMP_CYCLES_PUMP_TIME_S_NAME "PcPts"
#define CONFIG_PUMP_CYCLES_NR_PUMP_CYCLES_NAME "PcNpc"
#define CONFIG_PUMP_CYCLES_TIMES_MINUTES_PER_DAY_NAME "PcTmpd"

static const char *TAG = "Config";

void load_configuration() {
  ESP_LOGI(TAG, "Load Configuration");
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open(CONFIG_STORAGE_NAMESPACE, NVS_READONLY, &my_handle);

  if (err == ESP_ERR_NVS_NOT_FOUND) {

    ESP_LOGI(TAG, "No config saved. save initial config");
    save_configuration();
    return;
  }

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_u8(my_handle, CONFIG_ID_NAME, &configuration.id));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_u16(my_handle, CONFIG_PUMP_CYCLES_PUMP_TIME_S_NAME,
                  &configuration.pump_cycles.pump_time_s));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_u16(my_handle, CONFIG_PUMP_CYCLES_NR_PUMP_CYCLES_NAME,
                  &configuration.pump_cycles.nr_pump_cycles));

  size_t size = sizeof(configuration.pump_cycles.times_minutes_per_day);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_blob(my_handle, CONFIG_PUMP_CYCLES_TIMES_MINUTES_PER_DAY_NAME,
                   configuration.pump_cycles.times_minutes_per_day, &size));

  nvs_close(my_handle);
}

void save_configuration() {
  ESP_LOGI(TAG, "Save Configuration");
  nvs_handle_t my_handle;
  ESP_ERROR_CHECK(
      nvs_open(CONFIG_STORAGE_NAMESPACE, NVS_READWRITE, &my_handle));

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_u8(my_handle, CONFIG_ID_NAME, configuration.id));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_u16(my_handle, CONFIG_PUMP_CYCLES_PUMP_TIME_S_NAME,
                  configuration.pump_cycles.pump_time_s));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_u16(my_handle, CONFIG_PUMP_CYCLES_NR_PUMP_CYCLES_NAME,
                  configuration.pump_cycles.nr_pump_cycles));

  size_t size = sizeof(configuration.pump_cycles.times_minutes_per_day);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_blob(my_handle, CONFIG_PUMP_CYCLES_TIMES_MINUTES_PER_DAY_NAME,
                   configuration.pump_cycles.times_minutes_per_day, size));

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(my_handle));
  nvs_close(my_handle);
}
