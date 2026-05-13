#include "driver_hc_sr04.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>

#define WAIT_FOR_ECHO_HIGH_TIMEOUT_US 1000000 // 1 second timeout
#define WAIT_FOR_ECHO_LOW_TIMEOUT_US 1000000  // 1 second timeout

// Speed of sound at 20 °C is approximately 343.2 m/s, which is 0.3432 mm/µs
// divided by 2 considering round trip.
#define SONIC_SPEED_MM_PER_MUS_2 0.1716

static const char *TAG = "hc_sr04";

static portMUX_TYPE hc_sr04_critical_section_mutex =
    portMUX_INITIALIZER_UNLOCKED;

void hc_sr04_init() {
  // Initialize GPIO pins for trigger and echo
  gpio_config_t trigger_io_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_OUTPUT,
      .pin_bit_mask = (1ULL << CONFIG_HC_SR04_TRIGGER_PIN),
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&trigger_io_conf));
  gpio_set_level(CONFIG_HC_SR04_TRIGGER_PIN, 0); // Ensure trigger pin is low

  gpio_config_t echo_io_conf = {
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = (1ULL << CONFIG_HC_SR04_ECHO_PIN),
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&echo_io_conf));
}

esp_err_t hc_sr04_get_distance_mm(float *distance_mm) {
  taskENTER_CRITICAL(&hc_sr04_critical_section_mutex);
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(CONFIG_HC_SR04_TRIGGER_PIN, 1));
  const int64_t trigger_start_time = esp_timer_get_time();
  while (esp_timer_get_time() - trigger_start_time <= 10) {
    // ~10 µs trigger pulse — keep in critical section
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(CONFIG_HC_SR04_TRIGGER_PIN, 0));

  if (gpio_get_level(CONFIG_HC_SR04_ECHO_PIN) == 1) {
    taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
    ESP_LOGE(TAG, "Echo pin is already high after triggering measurement");
    hc_sr04_init();
    return ESP_FAIL;
  }

  const int64_t start_waiting_time = esp_timer_get_time();
  while (gpio_get_level(CONFIG_HC_SR04_ECHO_PIN) == 0) {
    if (esp_timer_get_time() - start_waiting_time >
        WAIT_FOR_ECHO_HIGH_TIMEOUT_US) {
      taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
      ESP_LOGE(TAG, "Timeout waiting for echo to go high.");
      hc_sr04_init();
      return ESP_ERR_TIMEOUT;
    }
  }
  const int64_t echo_start_time = esp_timer_get_time();
  while (gpio_get_level(CONFIG_HC_SR04_ECHO_PIN) == 1) {
    if (esp_timer_get_time() - echo_start_time > WAIT_FOR_ECHO_LOW_TIMEOUT_US) {
      taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
      ESP_LOGE(TAG, "Timeout waiting for echo to go low");
      hc_sr04_init();
      return ESP_ERR_TIMEOUT;
    }
  }
  const int64_t echo_end_time = esp_timer_get_time();
  taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);

  *distance_mm = (echo_end_time - echo_start_time) * SONIC_SPEED_MM_PER_MUS_2;
  if (*distance_mm < 20.0 || *distance_mm > 4000.0) {
    ESP_LOGI(TAG, "Measured distance out of range: %.2f mm", *distance_mm);
    return ESP_FAIL;
  }

  return ESP_OK;
}
