#include "esp_event.h"
#include <esp_err.h>

#include "init_utils.c"

#include "configuration.h"
#include "ota_updater.h"
#include "wifi_utils.h"

void app_main(void) {
  // Initialize storage
  initialize_nvs();

  load_configuration();

  // Create Event Loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // Initialize and connect to Wifi
  wifi_utils_init();
  wifi_utils_init_sntp();

  // run ota updater task
  initialize_ota_updater();
  xTaskCreate(&ota_updater_task, "ota_updater_task", 1024 * 8, NULL, 5, NULL);
}
