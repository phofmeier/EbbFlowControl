#include "driver_hc_sr04.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>

#define WAIT_FOR_ECHO_HIGH_TIMEOUT_US 1000000 // 1 second timeout
#define WAIT_FOR_ECHO_LOW_TIMEOUT_US 1000000  // 1 second timeout
#define SONIC_SPEED_CM_PER_S 34320.0          // Speed of sound in cm/s at 20 °C

static const char *TAG = "hc_sr04";

static portMUX_TYPE hc_sr04_critical_section_mutex =
    portMUX_INITIALIZER_UNLOCKED;

void hc_sr04_init() {
  // Initialize GPIO pins for trigger and echo
  // This is a placeholder implementation. You should set the actual GPIO pins
  // and configure them as needed (e.g., output for trigger, input for echo).
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

esp_err_t hc_sr04_get_distance_cm(float *distance_cm) {
  taskENTER_CRITICAL(&hc_sr04_critical_section_mutex);
  // Set trigger to high
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(CONFIG_HC_SR04_TRIGGER_PIN, 1));
  // wait for 10 micro seconds
  esp_rom_delay_us(11); // Also seen code with 5 us
  // set trigger to low
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(CONFIG_HC_SR04_TRIGGER_PIN, 0));

  if (gpio_get_level(CONFIG_HC_SR04_ECHO_PIN) == 1) {
    taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
    ESP_LOGE(TAG, "Echo pin is already high after triggering measurement");
    return ESP_FAIL; // Echo pin should be low after triggering
  }

  const int start_waiting_time = esp_timer_get_time();
  while (gpio_get_level(CONFIG_HC_SR04_ECHO_PIN) == 0) {
    // wait for echo to go high with timeout  Todo: which timeout?
    if (esp_timer_get_time() - start_waiting_time >
        WAIT_FOR_ECHO_HIGH_TIMEOUT_US) {
      taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
      ESP_LOGE(TAG, "Timeout waiting for echo to go high");
      return ESP_ERR_TIMEOUT; // Timeout waiting for echo to go high
    }
  }
  const int echo_start_time = esp_timer_get_time();
  while (gpio_get_level(CONFIG_HC_SR04_ECHO_PIN) == 1) {
    // wait for echo to go low with timeout
    if (esp_timer_get_time() - echo_start_time > WAIT_FOR_ECHO_LOW_TIMEOUT_US) {
      taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
      ESP_LOGE(TAG, "Timeout waiting for echo to go low");
      return ESP_ERR_TIMEOUT; // Timeout waiting for echo to go low
    }
  }
  const int echo_end_time = esp_timer_get_time();
  taskEXIT_CRITICAL(&hc_sr04_critical_section_mutex);
  // calculate distance based on sonic speed 34320 cm/s bei 20 °C -> distance =
  // duration_s * 34320 / 2
  *distance_cm =
      ((echo_end_time - echo_start_time) / 1e6) * SONIC_SPEED_CM_PER_S / 2.0;
  if (*distance_cm < 2.0 || *distance_cm > 400) {
    ESP_LOGI(TAG, "Measured distance out of range: %.2f cm", *distance_cm);
    return ESP_FAIL;
  }

  return ESP_OK;
}

// add comment on how to temp compensate for sonic speed if needed, e.g. based
// on temperature sensor or fixed value
