#include "state.h"

#include "esp_err.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "State";

#define CONFIG_MAX_NUMBER_TASK_TO_NOTIFY 5

// Initial state
struct state_t global_state = {.pumping_state = {.pump_on = 0}};

// Array containing all task handles which need to be notified
static TaskHandle_t tasks_to_notify[CONFIG_MAX_NUMBER_TASK_TO_NOTIFY];
// Number of task handles in the array.
static uint16_t nr_task_to_notify = 0;

void set_pump_on(int pump_on) {
  if (global_state.pumping_state.pump_on != pump_on) {
    global_state.pumping_state.pump_on = pump_on;
    ESP_LOGI(TAG, "Pump state changed to: %s", pump_on ? "ON" : "OFF");
    handle_state_changed_callbacks();
  }
}

void handle_state_changed_callbacks() {
  for (size_t i = 0; i < nr_task_to_notify; i++) {
    xTaskNotifyGive(tasks_to_notify[i]);
  }
}

esp_err_t add_notify_for_state_change(TaskHandle_t task) {
  if (nr_task_to_notify >= CONFIG_MAX_NUMBER_TASK_TO_NOTIFY) {
    return ESP_ERR_NO_MEM;
  }

  tasks_to_notify[nr_task_to_notify] = task;
  nr_task_to_notify++;
  return ESP_OK;
}
