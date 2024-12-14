#include "pump_control.h"

#include "configuration.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

/**
 * @brief Stop pumping
 *
 */
inline void stop_pump() { gpio_set_level(CONFIG_PUMP_GPIO_OUTPUT_PIN, 0); }

/**
 * @brief Stop pumping
 *
 */
inline void start_pump() { gpio_set_level(CONFIG_PUMP_GPIO_OUTPUT_PIN, 1); }

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
  io_conf.pin_bit_mask = CONFIG_PUMP_GPIO_OUTPUT_PIN;
  // disable pull-down mode
  io_conf.pull_down_en = 0;
  // disable pull-up mode
  io_conf.pull_up_en = 0;
  // configure GPIO with the given settings
  gpio_config(&io_conf);
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
#define STACK_SIZE 200

/* Structure that will hold the TCB of the task being created. */

StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
StackType_t xStack[STACK_SIZE];

void pump_control_task(void *pvParameters) {
  // initialize pump
  configure_pump_output();
  stop_pump();

  // setup task
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
  signed short last_run = get_cur_min_of_day();
  bool pumping = false;
  time_t pumping_start_time;
  time_t now;

  for (;;) {
    if (pumping) {
      // check if we need to stop
      time(&now);
      const double time_diff_s = difftime(now, pumping_start_time);
      if (time_diff_s >= configuration.pump_cycles.pump_time_s) {
        stop_pump();
        pumping = false;
      }
    } else {

      const unsigned short curr_min = get_cur_min_of_day();
      const unsigned short nr_pump_cycles =
          configuration.pump_cycles.nr_pump_cycles;
      const unsigned short *times_minutes_per_day =
          configuration.pump_cycles.times_minutes_per_day;
      if (curr_min < last_run) {
        // Next day -> reset last run
        last_run = -1;
      }

      for (size_t i = 0; i < nr_pump_cycles; i++) {
        // Check each possible config for starting
        if (times_minutes_per_day[i] > last_run &&
            curr_min >= times_minutes_per_day[i]) {
          // current config never run and its time -> start pump
          pumping = true;
          last_run = times_minutes_per_day[i];
          time(&pumping_start_time); // update start time
          start_pump();
          break;
        }
      }
    }

    vTaskDelay(xDelay); // wait for next cycle
  }
}

void create_pump_control_task() {

  // Static task without dynamic memory allocation
  xTaskCreateStatic(
      pump_control_task, "PumpControl", /* Task Name */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* No Parameter */
      tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure. */
}
