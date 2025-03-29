#include "pump_control.h"

#include "cJSON.h"
#include "configuration.h"
#include "data_logging.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

// Bitmask to change the configuration for the gpio pin
#define PUMP_GPIO_BITMASK (1ULL << CONFIG_PUMP_GPIO_OUTPUT_PIN)

static const char *TAG = "pump_control";

/**
 * @brief Stop pumping
 *
 */
void stop_pump() {
  gpio_set_level(CONFIG_PUMP_GPIO_OUTPUT_PIN, 1);
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "status", "stop");
  send_timed_data(CONFIG_MQTT_PUMP_STATUS_TOPIC, data);
}

/**
 * @brief Start pumping
 *
 */
void start_pump() {
  gpio_set_level(CONFIG_PUMP_GPIO_OUTPUT_PIN, 0);
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "status", "start");
  send_timed_data(CONFIG_MQTT_PUMP_STATUS_TOPIC, data);
}

/**
 * @brief Configure the GPIO output for the pump
 *
 */
void configure_pump_output() {
  // zero-initialize the config structure.
  gpio_config_t io_conf = {};
  // disable interrupt
  io_conf.intr_type = GPIO_INTR_DISABLE;
  // set as output mode
  io_conf.mode = GPIO_MODE_OUTPUT;
  // bit mask of the pins that you want to set,e.g.GPIO18/19
  io_conf.pin_bit_mask = PUMP_GPIO_BITMASK;
  // disable pull-down mode
  io_conf.pull_down_en = 0;
  // disable pull-up mode
  io_conf.pull_up_en = 0;
  // configure GPIO with the given settings
  ESP_ERROR_CHECK(gpio_config(&io_conf));
  gpio_set_level(CONFIG_PUMP_GPIO_OUTPUT_PIN, 1);
}

/**
 * @brief Get the current minute of the day.
 *
 * The configured local timezone is used. The number represents the minutes
 * since the start of the day.
 *
 * @return unsigned short minutes since start of the local day
 */
unsigned short get_cur_min_of_day() {
  // set timezone
  setenv("TZ", CONFIG_LOCAL_TIME_ZONE, 1);
  tzset();

  // get local time
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

/* Dimensions of the buffer that the task being created will use as its stack.
   NOTE: This is the number of words the stack will hold, not the number of
   bytes. For example, if each stack item is 32-bits, and this is set to 100,
   then 400 bytes (100 * 32-bits) will be allocated. */
#define STACK_SIZE 4096

/* Structure that will hold the TCB of the task being created. */

static StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
static StackType_t xStack[STACK_SIZE];

/**
 * @brief Enum with the current state of the pump controller.
 *
 */
enum State {
  PUMPING, // Currently pumping
  WAITING, // Waiting for next pump cycle
};

void pump_control_task(void *pvParameters) {
  // setup task
  signed short last_run = get_cur_min_of_day();
  enum State state = WAITING;
  time_t pumping_start_time;
  time_t now;

  for (;;) {
    switch (state) {
    case PUMPING:
      time(&now);
      const double time_diff_s = difftime(now, pumping_start_time);
      if (time_diff_s >= configuration.pump_cycles.pump_time_s) {
        stop_pump();
        state = WAITING;
        ESP_LOGI(TAG, "Stop pump");
      } else {
        // wait for stop
        const int wait_time_s =
            floor((configuration.pump_cycles.pump_time_s - time_diff_s) * 0.9);
        ESP_LOGI(TAG, "Wait for stop for %i s", wait_time_s);
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_time_s * 1e3 + 100));
      }
      break;

    case WAITING:
      const unsigned short curr_min = get_cur_min_of_day();
      const unsigned short nr_pump_cycles =
          configuration.pump_cycles.nr_pump_cycles;
      const unsigned short *times_minutes_per_day =
          configuration.pump_cycles.times_minutes_per_day;
      if (curr_min < last_run) {
        // Next day -> reset last run
        last_run = -1;
      }
      // initialize with waiting for next day
      int minutes_to_next_start = 24 * 60 - curr_min;
      for (size_t i = 0; i < nr_pump_cycles; i++) {
        // Check each possible config for starting
        if (times_minutes_per_day[i] > last_run) {
          // This config was not run yet
          const int min_to_start = times_minutes_per_day[i] - curr_min;
          if (minutes_to_next_start > min_to_start) {
            minutes_to_next_start = min_to_start;
          }
          if (min_to_start <= 0) {
            // current config never run and its time -> start pump
            state = PUMPING;
            last_run = times_minutes_per_day[i];
            time(&pumping_start_time); // update start time
            start_pump();
            ESP_LOGI(TAG, "Start pump, curr_min=%i, i=%i, conf=%i", curr_min, i,
                     times_minutes_per_day[i]);
            break;
          }
        }
      }
      // Wait for next start
      if (state == WAITING) {
        minutes_to_next_start = floor(minutes_to_next_start * 0.9);
        minutes_to_next_start = fmax(0, minutes_to_next_start);
        ESP_LOGI(TAG, "Wait for stop for %i min", minutes_to_next_start);
        // Wait for minimum 10 second
        ulTaskNotifyTake(
            pdTRUE, pdMS_TO_TICKS((minutes_to_next_start * 60 + 10) * 1e3));
      }
      break;
    }
  }
}

TaskHandle_t create_pump_control_task() {
  // initialize GPIO
  configure_pump_output();
  stop_pump();

  // Static task without dynamic memory allocation
  TaskHandle_t task_handle = xTaskCreateStatic(
      pump_control_task, "PumpControl", /* Task Name */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* No Parameter */
      tskIDLE_PRIORITY + 2, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure. */
  return task_handle;
}
