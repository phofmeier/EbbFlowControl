#ifndef COMPONENTS_STATE_INCLUDE_STATE
#define COMPONENTS_STATE_INCLUDE_STATE

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

struct pumping_state_t {
  int pump_on;
};

struct state_t {
  struct pumping_state_t pumping_state;
};

/**
 * @brief Declaration of the global state
 *
 */
extern struct state_t global_state;

/**
 * @brief Set the pump on/off state.
 * @param pump_on 1 to turn the pump on, 0 to turn it off
 */
void set_pump_on(int pump_on);

/**
 * @brief handle all callbacks after the state changed.
 *
 */
void handle_state_changed_callbacks();

/**
 * @brief Add a task handle which is notified by `xTaskNotifyGive` when the
 * state is changed.
 *
 * @param task the task handle of the task to notify
 * @return esp_err_t `ESP_ERR_NO_MEM` if more than
 * `CONFIG_MAX_NUMBER_TASK_TO_NOTIFY` are tried to be added else ESP_OK
 */
esp_err_t add_notify_for_state_change(TaskHandle_t task);

#endif /* COMPONENTS_STATE_INCLUDE_STATE */
