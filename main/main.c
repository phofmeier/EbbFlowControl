#include "esp_event.h"
#include <esp_err.h>

#include "init_utils.c"

#include "configuration.h"
#include "data_logging.h"
#include "mqtt5_connection.h"
#include "ota_scheduler.h"
#include "ota_updater.h"
#include "pump_control.h"
#include "wifi_utils.h"

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

  // Might wait up to 24 hours for the first update.
  create_ota_scheduler_task();

  // After running for 25 hours without any errors we can mark it valid.
  vTaskDelay(pdMS_TO_TICKS(25 * 60 * 60 * 1e3));
  mark_running_app_version_valid();
}
