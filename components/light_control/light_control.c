#include "light_control.h"

#include "driver_gp8211s.h"
#include <stdio.h>

#define TESTING_LIGHT_CONTROL 1

static const char *TAG = "light_control";

void initialize_light_control() {
  gp8211s_init_i2c();
  gp8211s_init_device();
  gp8211s_set_output(0); // Start with light off
}

void light_test() {
  // Example: Ramp up light intensity from 0 to max over 10 seconds
  for (uint16_t value = 0; value <= 0x7FFF; value += 0x0100) {
    gp8211s_set_output(value);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second between steps
  }
  // Hold at max intensity for 10 seconds
  vTaskDelay(pdMS_TO_TICKS(10000));
  // Ramp down light intensity from max to 0 over 10 seconds
  for (int16_t value = 0x7FFF; value >= 0; value -= 0x0100) {
    gp8211s_set_output(value);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second between steps
  }
  // Hold at off for 10 seconds
  vTaskDelay(pdMS_TO_TICKS(10000));
}

void light_control_task(void *pvParameters) {
  while (1) {
    if (TESTING_LIGHT_CONTROL) {
      light_test();
    } else {
      // Normal light control logic would go here
    }
  }
}

TaskHandle_t create_light_control_task() {
  TaskHandle_t task_handle;
  xTaskCreate(light_control_task, "light_control_task", 4096, NULL, 5,
              &task_handle);
  return task_handle;
}
