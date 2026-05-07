#include "level_sensor.h"
#include "data_logging.h"
#include "driver_hc_sr04.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdint.h>

static const char *TAG = "level_sensor";
static const TickType_t LEVEL_SENSOR_MEASUREMENT_DELAY =
    pdMS_TO_TICKS(CONFIG_LEVEL_SENSOR_UPDATE_INTERVAL * 60 * 1000);

void level_sensor_init(void) { hc_sr04_init(); }

static void level_sensor_task(void *pvParameters) {
  (void)pvParameters;
  while (true) {
    float measured_distance_cm = 0.0f;
    esp_err_t err = hc_sr04_get_distance_cm(&measured_distance_cm);
    if (err == ESP_OK) {
      uint32_t measured_distance_mm =
          (uint32_t)roundf(measured_distance_cm * 10.0f);
      ESP_LOGI(TAG, "Level distance: %u mm", measured_distance_mm);
      add_level_data_item(measured_distance_mm);
    } else {
      ESP_LOGW(TAG, "HC-SR04 measurement failed: %s", esp_err_to_name(err));
    }
    vTaskDelay(LEVEL_SENSOR_MEASUREMENT_DELAY);
  }
}

TaskHandle_t create_level_sensor_task(void) {
  TaskHandle_t handle = NULL;
  BaseType_t result = xTaskCreate(level_sensor_task, "level_sensor",
                                  configMINIMAL_STACK_SIZE + 2048, NULL,
                                  tskIDLE_PRIORITY + 1, &handle);
  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create level sensor task");
    return NULL;
  }
  return handle;
}
