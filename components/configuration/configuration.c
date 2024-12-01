#include "configuration.h"
#include "cJSON.h"
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

void set_config_from_json(const char *json, const size_t json_length) {
  cJSON *root = cJSON_ParseWithLength(json, json_length);
  if (root == NULL) {
    ESP_LOGI(TAG, "Can not parse config json");
    return;
  }

  cJSON *pump_cycles = cJSON_GetObjectItem(root, "pump_cycles");
  if (pump_cycles != NULL) {
    cJSON *pump_time_s = cJSON_GetObjectItem(pump_cycles, "pump_time_s");
    if (pump_time_s != NULL) {
      configuration.pump_cycles.pump_time_s = pump_time_s->valueint;
    }

    cJSON *nr_pump_cycles = cJSON_GetObjectItem(pump_cycles, "nr_pump_cycles");
    cJSON *times_minutes_per_day =
        cJSON_GetObjectItem(pump_cycles, "times_minutes_per_day");

    if (nr_pump_cycles != NULL && times_minutes_per_day != NULL) {
      const int size_array = cJSON_GetArraySize(times_minutes_per_day);
      if (size_array == nr_pump_cycles->valueint &&
          size_array <= CONFIG_MAX_NUMBER_PUMP_CYCLES_PER_DAY) {
        configuration.pump_cycles.nr_pump_cycles = nr_pump_cycles->valueint;

        for (size_t i = 0; i < size_array; i++) {
          configuration.pump_cycles.times_minutes_per_day[i] =
              cJSON_GetArrayItem(times_minutes_per_day, i)->valueint;
        }
      }
    }
    ESP_LOGI(TAG, "New Configuration set");
  }
  cJSON_Delete(root);
  save_configuration();
}
