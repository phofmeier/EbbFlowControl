#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include <esp_err.h>

#include "init_utils.c"

#include "configuration.h"
#include "data_logging.h"
#include "mqtt5_connection.h"
#include "ota_scheduler.h"
#include "ota_updater.h"
#include "pump_control.h"
#include "wifi_utils_sntp.h"
#include "wifi_utils_sta.h"

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
  ESP_ERROR_CHECK(esp_netif_init());
  wifi_utils_init();
  ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_utils_connect_wifi_blocking());
  wifi_utils_init_sntp();
  wifi_utils_create_connection_checker_task();

  // MQTT Setup
  mqtt5_conn_init();
  mqtt5_create_connection_checker_task();
  // Data Logging Setup
  create_data_logging_task();
  // Create control tasks
  ESP_ERROR_CHECK(add_notify_for_new_config(create_pump_control_task()));

  // Might wait up to 24 hours for the first update.
  create_ota_scheduler_task();

  // Mark config as valid after successful wifi and mqtt connection

  // After running for 25 hours without any errors we can mark it valid.
  static const uint32_t initial_delay_ticks = 25 * 60 * 60 * configTICK_RATE_HZ;
  vTaskDelay(initial_delay_ticks);
  while (configuration.network.valid_bits !=
         (NETWORK_WIFI_VALID_BIT | NETWORK_MQTT_VALID_BIT)) {
    vTaskDelay(pdMS_TO_TICKS(1000 * 60 * 60)); // check every hour
  }
  mark_running_app_version_valid();
}
