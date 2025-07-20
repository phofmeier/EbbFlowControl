#include "configuration.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <stdio.h>

// Define names which are used for the persistent storage
#define CONFIG_STORAGE_NAMESPACE "EFCConf"
#define CONFIG_ID_NAME "id"
#define CONFIG_PUMP_CYCLES_PUMP_TIME_S_NAME "PcPts"
#define CONFIG_PUMP_CYCLES_NR_PUMP_CYCLES_NAME "PcNpc"
#define CONFIG_PUMP_CYCLES_TIMES_MINUTES_PER_DAY_NAME "PcTmpd"
#define CONFIG_WIFI_SSID_NAME "NetSsid"
#define CONFIG_WIFI_PASSWORD_NAME "NetPass"
#define CONFIG_MQTT_BROKER_NAME "NetMqttB"
#define CONFIG_MQTT_USERNAME_NAME "NetMqttU"
#define CONFIG_MQTT_PASSWORD_NAME "NetMqttP"
// Maximum number of task to be notified if the config changes
#define CONFIG_MAX_NUMBER_TASK_TO_NOTIFY 20

static const char *TAG = "Config";

// Initial configuration
struct configuration_t configuration = {
    .id = CONFIG_DEVICE_ID,
    .pump_cycles =
        {
            .pump_time_s = 120,
            .nr_pump_cycles = 3,
            .times_minutes_per_day = {6 * 60, 12 * 60, 20 * 60},
        },
    .network =
        {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .mqtt_broker = CONFIG_MQTT_BROKER_URI,
            .mqtt_username = CONFIG_MQTT_USERNAME,
            .mqtt_password = CONFIG_MQTT_PASSWORD,
        },
};

// Array containing all task handles which need to be notified
TaskHandle_t tasks_to_notify[CONFIG_MAX_NUMBER_TASK_TO_NOTIFY];
// Number of task handles in the array.
uint16_t nr_task_to_notify = 0;

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

  // Load network configuration
  size_t wifi_ssid_length = sizeof(configuration.network.ssid);
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_str(my_handle, CONFIG_WIFI_SSID_NAME,
                                            configuration.network.ssid,
                                            &wifi_ssid_length));
  size_t wifi_password_length = sizeof(configuration.network.password);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_str(my_handle, CONFIG_WIFI_PASSWORD_NAME,
                  configuration.network.password, &wifi_password_length));
  size_t mqtt_broker_length = sizeof(configuration.network.mqtt_broker);
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_get_str(my_handle, CONFIG_MQTT_BROKER_NAME,
                                            configuration.network.mqtt_broker,
                                            &mqtt_broker_length));
  size_t mqtt_username_length = sizeof(configuration.network.mqtt_username);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_str(my_handle, CONFIG_MQTT_USERNAME_NAME,
                  configuration.network.mqtt_username, &mqtt_username_length));
  size_t mqtt_password_length = sizeof(configuration.network.mqtt_password);
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_get_str(my_handle, CONFIG_MQTT_PASSWORD_NAME,
                  configuration.network.mqtt_password, &mqtt_password_length));

  nvs_close(my_handle);
}

void save_configuration() {
  ESP_LOGI(TAG, "Save Configuration");
  nvs_handle_t my_handle;
  ESP_ERROR_CHECK(
      nvs_open(CONFIG_STORAGE_NAMESPACE, NVS_READWRITE, &my_handle));

  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_u8(my_handle, CONFIG_ID_NAME, configuration.id));

  // Pump cycles
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

  // Network configuration
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(my_handle, CONFIG_WIFI_SSID_NAME,
                                            configuration.network.ssid));
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(
      my_handle, CONFIG_WIFI_PASSWORD_NAME, configuration.network.password));
  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_str(my_handle, CONFIG_MQTT_BROKER_NAME,
                                            configuration.network.mqtt_broker));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_str(my_handle, CONFIG_MQTT_USERNAME_NAME,
                  configuration.network.mqtt_username));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      nvs_set_str(my_handle, CONFIG_MQTT_PASSWORD_NAME,
                  configuration.network.mqtt_password));

  ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(my_handle));
  nvs_close(my_handle);
}

void set_config_from_json(const char *json, const size_t json_length) {
  cJSON *root = cJSON_ParseWithLength(json, json_length);
  if (root == NULL) {
    ESP_LOGI(TAG, "Can not parse config json");
    return;
  }

  const cJSON *id = cJSON_GetObjectItem(root, "id");
  if (id != NULL && id->valueint != configuration.id) {
    ESP_LOGI(TAG, "Configuration for different board id %u arrived.",
             id->valueint);
    cJSON_Delete(root);
    return;
  }

  const cJSON *pump_cycles = cJSON_GetObjectItem(root, "pump_cycles");
  if (pump_cycles != NULL) {
    // Pump cycles key in json
    const cJSON *pump_time_s = cJSON_GetObjectItem(pump_cycles, "pump_time_s");
    if (pump_time_s != NULL) {
      configuration.pump_cycles.pump_time_s = pump_time_s->valueint;
    }

    const cJSON *nr_pump_cycles =
        cJSON_GetObjectItem(pump_cycles, "nr_pump_cycles");
    const cJSON *times_minutes_per_day =
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
  handle_new_configuration_callbacks();
}

cJSON *get_config_as_json() {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "id", configuration.id);

  cJSON *pump_cycles = cJSON_AddObjectToObject(root, "pump_cycles");
  cJSON_AddNumberToObject(pump_cycles, "pump_time_s",
                          configuration.pump_cycles.pump_time_s);
  cJSON_AddNumberToObject(pump_cycles, "nr_pump_cycles",
                          configuration.pump_cycles.nr_pump_cycles);

  cJSON *times_minutes_per_day_json_arr =
      cJSON_AddArrayToObject(pump_cycles, "times_minutes_per_day");
  for (size_t i = 0; i < configuration.pump_cycles.nr_pump_cycles; i++) {
    cJSON_AddItemToArray(
        times_minutes_per_day_json_arr,
        cJSON_CreateNumber(configuration.pump_cycles.times_minutes_per_day[i]));
  }

  return root;
}

void handle_new_configuration_callbacks() {
  for (size_t i = 0; i < nr_task_to_notify; i++) {
    xTaskNotifyGive(tasks_to_notify[i]);
  }
}

esp_err_t add_notify_for_new_config(TaskHandle_t task) {
  if (nr_task_to_notify >= CONFIG_MAX_NUMBER_TASK_TO_NOTIFY) {
    return ESP_ERR_NO_MEM;
  }

  tasks_to_notify[nr_task_to_notify] = task;
  nr_task_to_notify++;
  return ESP_OK;
}
