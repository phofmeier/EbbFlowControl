#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "pump_control.h"

#include "configuration.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define GPIO_PUMP_OUTPUT_PIN 8

void configure_pump_output() {
  // zero-initialize the config structure.
  gpio_config_t io_conf = {};
  // disable interrupt
  io_conf.intr_type = GPIO_INTR_DISABLE;
  // set as output mode
  io_conf.mode = GPIO_MODE_OUTPUT;
  // bit mask of the pins that you want to set,e.g.GPIO18/19
  io_conf.pin_bit_mask = GPIO_PUMP_OUTPUT_PIN;
  // disable pull-down mode
  io_conf.pull_down_en = 0;
  // disable pull-up mode
  io_conf.pull_up_en = 0;
  // configure GPIO with the given settings
  gpio_config(&io_conf);
}

unsigned short get_cur_min_of_day() {
  time_t now;
  struct tm timeinfo;
  // Set timezone to Germany Berlin
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  time(&now);
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

/* Function that implements the task being created. */
void pump_control_task(void *pvParameters) {
  configure_pump_output();
  gpio_set_level(GPIO_PUMP_OUTPUT_PIN, 0);
  const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
  signed short last_run = get_cur_min_of_day();
  bool pumping = false;
  time_t start_time;
  time_t now;

  for (;;) {
    if (pumping) {
      time(&now);
      double diff_t = difftime(now, start_time);
      const unsigned short pump_time_s = configuration.pump_cycles.pump_time_s;
      if (diff_t >= pump_time_s) {
        gpio_set_level(GPIO_PUMP_OUTPUT_PIN, 0);
        pumping = false;
      }
    } else {
      unsigned short curr_min = get_cur_min_of_day();
      const unsigned short nr_pump_cycles =
          configuration.pump_cycles.nr_pump_cycles;
      const unsigned short *times_minutes_per_day =
          configuration.pump_cycles.times_minutes_per_day;
      if (curr_min < last_run) {
        last_run = -1;
      }
      for (size_t i = 0; i < nr_pump_cycles; i++) {
        if (times_minutes_per_day[i] > last_run &&
            curr_min >= times_minutes_per_day[i]) {
          // start_running
          pumping = true;
          last_run = times_minutes_per_day[i];
          time(&start_time);
          gpio_set_level(GPIO_PUMP_OUTPUT_PIN, 1);
          break;
        }
      }
    }

    vTaskDelay(xDelay);
  }
}

/* Function that creates a task. */

void create_pump_control_task() {

  /* Create the task without using any dynamic memory allocation. */
  xTaskCreateStatic(
      pump_control_task, "PumpControl", /* Text name for the task. */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* Parameter passed into the task. */
      tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure. */

  /* puxStackBuffer and pxTaskBuffer were not NULL, so the task will have

     been created, and xHandle will be the task's handle. Use the handle

     to suspend the task. */

  // [vTaskSuspend](/ Documentation / 02 - Kernel / 04 - API - references / 02 -
  // Task - control / 06 - vTaskSuspend)(xHandle);
}