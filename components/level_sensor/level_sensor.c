#include "level_sensor.h"
#include "configuration.h"
#include "data_logging.h"
#include "driver_hc_sr04.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state.h"
#include <esp_timer.h>
#include <math.h>
#include <stdint.h>

static const char *TAG = "level_sensor";

#define SEC_TO_US(x) (int64_t)((x) * 1000000LL)
#define MIN_TO_US(x) (int64_t)((x) * 60LL * 1000000LL)

#define LEVEL_SENSOR_PUMPING_MEASUREMENT_FREQUENCY_US SEC_TO_US(10)
#define LEVEL_SENSOR_RESTING_MEASUREMENT_FREQUENCY_US MIN_TO_US(30)

#define MEDIAN_FILTER_SIZE 5
#define MAX_MEASUREMENT_ATTEMPTS 10

/***
 * @brief Measure distance using the HC-SR04 sensor with a median filter to
 * improve accuracy.
 */
static esp_err_t measure_distance_filtered_mm(uint16_t *distance_mm) {
  uint16_t distance_filter_values[MEDIAN_FILTER_SIZE] = {0};
  size_t distance_filter_index = 0;

  for (size_t i = 0; i < MAX_MEASUREMENT_ATTEMPTS &&
                     distance_filter_index < MEDIAN_FILTER_SIZE;
       i++) {
    float measured_distance_mm_raw = 0.0f;
    esp_err_t err = hc_sr04_get_distance_mm(&measured_distance_mm_raw);
    if (err == ESP_OK) {
      const uint16_t measured_distance_mm =
          (uint16_t)roundf(measured_distance_mm_raw);
      distance_filter_values[distance_filter_index] = measured_distance_mm;
      ESP_LOGD(TAG, "Measured distance: %u mm", measured_distance_mm);
      distance_filter_index++;
    } else {
      ESP_LOGW(TAG, "HC-SR04 measurement failed: %s", esp_err_to_name(err));
    }
    vTaskDelay(
        pdMS_TO_TICKS(100)); // Max Snensor frequency is 40 Hz, so wait at least
                             // 25 ms between measurements. Using 100 ms to be
                             // safe and reduce sensor stress.
  }

  if (distance_filter_index != MEDIAN_FILTER_SIZE) {
    ESP_LOGW(TAG, "Failed to get enough valid measurements for median filter");

    return ESP_FAIL;
  }

  // Sort the measurements to find the median
  for (size_t i = 0; i < MEDIAN_FILTER_SIZE - 1; i++) {
    for (size_t j = 0; j < MEDIAN_FILTER_SIZE - i - 1; j++) {
      if (distance_filter_values[j] > distance_filter_values[j + 1]) {
        uint16_t temp = distance_filter_values[j];
        distance_filter_values[j] = distance_filter_values[j + 1];
        distance_filter_values[j + 1] = temp;
      }
    }
  }

  uint16_t median_distance_mm = distance_filter_values[MEDIAN_FILTER_SIZE / 2];
  *distance_mm = median_distance_mm;
  return ESP_OK;
}

enum level_sensor_state_t {
  PUMPING,
  BACKFLOW,
  RESTING,
};

static enum level_sensor_state_t current_sensor_state = RESTING;
static int64_t pump_stop_time = 0;

/***
 * @brief Calculate the measurement frequency based on the current sensor state.
 * During pumping we use the higher frequency. After pumping we assume two times
 * the pumping time the backflow which is also sampled with high frequency.
 * During resting a lower frequency is applied.
 */
static int64_t calculate_measurement_frequency_us() {
  if (global_state.pumping_state.pump_on) {
    current_sensor_state = PUMPING;
  } else {
    if (current_sensor_state == PUMPING) {
      // Pump just stopped.
      current_sensor_state = BACKFLOW;
      pump_stop_time = esp_timer_get_time();
      ESP_LOGD(TAG, "Pump stopped, switching to backflow state");
    } else if (current_sensor_state == BACKFLOW &&
               (esp_timer_get_time() - pump_stop_time) >
                   (2LL * configuration.pump_cycles.pump_time_s * 1000000LL)) {
      // Assume backflow is finished after 2 pump cycles
      current_sensor_state = RESTING;
      ESP_LOGD(TAG, "Assuming backflow finished, switching to resting state");
    }
  }

  switch (current_sensor_state) {
  case PUMPING:
  case BACKFLOW:
    return LEVEL_SENSOR_PUMPING_MEASUREMENT_FREQUENCY_US;
  case RESTING:
  default:
    return LEVEL_SENSOR_RESTING_MEASUREMENT_FREQUENCY_US;
  }
}

void level_sensor_init(void) { hc_sr04_init(); }

static void level_sensor_task(void *pvParameters) {
  int64_t next_measurement_time = esp_timer_get_time();
  while (true) {
    uint16_t measured_distance_mm = 0;
    const esp_err_t err = measure_distance_filtered_mm(&measured_distance_mm);
    if (err == ESP_OK) {
      ESP_LOGD(TAG, "Level distance: %u mm", measured_distance_mm);
      add_level_data_item(measured_distance_mm);
    } else {
      ESP_LOGW(TAG, "HC-SR04 measurement failed: %s", esp_err_to_name(err));
    }

    const int64_t frequency_us = calculate_measurement_frequency_us();
    next_measurement_time = next_measurement_time + frequency_us;
    const int64_t now = esp_timer_get_time();
    if (next_measurement_time <= now) {
      // We are already past the next measurement time. Just continue without
      // waiting.
      ESP_LOGD(TAG,
               "Next measurement time already passed (by %lld ms), measuring "
               "immediately",
               (now - next_measurement_time) / 1000);
      continue;
    }
    const int64_t wait_time_us = next_measurement_time - now;
    ESP_LOGD(TAG, "Waiting for %lld ms until next measurement",
             wait_time_us / 1000);

    // wait for next measurement or state change, whichever comes first.
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_time_us / 1000)) > 0) {
      // State changed, measure immediately.
      ESP_LOGD(TAG, "State changed, measuring immediately");
      next_measurement_time = esp_timer_get_time();
    }
  }
}

TaskHandle_t create_level_sensor_task(void) {
  TaskHandle_t handle = NULL;
  BaseType_t result = xTaskCreate(level_sensor_task, "level_sensor",
                                  configMINIMAL_STACK_SIZE + 2048, NULL,
                                  tskIDLE_PRIORITY + 2, &handle);
  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create level sensor task");
    return NULL;
  }
  return handle;
}
