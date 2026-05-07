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

#define MEDIAN_FILTER_SIZE 5
#define MAX_MEASUREMENT_ATTEMPTS 10

    /***
     * @brief Measure distance using the HC-SR04 sensor with a median filter to
     * improve accuracy.
     */
    esp_err_t measure_distance_filtered_mm(uint32_t *distance_mm) {
  uint32_t distance_filter_values[MEDIAN_FILTER_SIZE] = {0};
  size_t distance_filter_index = 0;

  for (size_t i = 0; i < MAX_MEASUREMENT_ATTEMPTS &&
                     distance_filter_index < MEDIAN_FILTER_SIZE;
       i++) {
    float measured_distance_cm = 0.0f;
    esp_err_t err = hc_sr04_get_distance_cm(&measured_distance_cm);
    if (err == ESP_OK) {
      uint32_t measured_distance_mm =
          (uint32_t)roundf(measured_distance_cm * 10.0f);
      distance_filter_values[distance_filter_index] = measured_distance_mm;
      ESP_LOGI(TAG, "Measured distance: %u mm", measured_distance_mm);
      distance_filter_index++;
    } else {
      ESP_LOGW(TAG, "HC-SR04 measurement failed: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (distance_filter_index != MEDIAN_FILTER_SIZE) {
    ESP_LOGW(TAG, "Failed to get enough valid measurements for median filter");

    return ESP_FAIL;
  }

  // Sort the measurements to find the median
  for (size_t i = 0; i < MEDIAN_FILTER_SIZE - 1; i++) {
    for (size_t j = 0; j < MEDIAN_FILTER_SIZE - i - 1; j++) {
      if (distance_filter_values[j] > distance_filter_values[j + 1]) {
        uint32_t temp = distance_filter_values[j];
        distance_filter_values[j] = distance_filter_values[j + 1];
        distance_filter_values[j + 1] = temp;
      }
    }
  }

  uint32_t median_distance_mm = distance_filter_values[MEDIAN_FILTER_SIZE / 2];
  *distance_mm = median_distance_mm;
  return ESP_OK;
}

void level_sensor_init(void) { hc_sr04_init(); }

static void level_sensor_task(void *pvParameters) {
  (void)pvParameters;
  while (true) {
    uint32_t measured_distance_mm = 0.0;
    esp_err_t err = measure_distance_filtered_mm(&measured_distance_mm);
    if (err == ESP_OK) {
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
