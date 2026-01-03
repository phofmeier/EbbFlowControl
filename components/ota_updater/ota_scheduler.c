#include "ota_scheduler.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <time.h>

#include "ota_updater.h"

static const char *TAG = "ota_scheduler";

void ota_scheduler_task(void *pvParameter) {
  // Note: If we restart at midnight we wait for 24h for the next update. So if
  // we restart due to any error after an update we do not immediately update
  // again.
  for (;;) {
    // Schedule next update somewhere between 0 and 1 o clock.
    setenv("TZ", CONFIG_LOCAL_TIME_ZONE, 1);
    tzset();
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    const int hours_until_midnight = 24 - timeinfo.tm_hour;
    ESP_LOGD(TAG, "Stack high water mark %d",
             uxTaskGetStackHighWaterMark(NULL));
    ESP_LOGI(TAG, "Wait for %i hours before next update.",
             hours_until_midnight);
    vTaskDelay(pdMS_TO_TICKS(hours_until_midnight * 60 * 60 * 1e3));

    // Try to update now.
    xTaskCreate(&ota_updater_task, "ota_updater_task", 1024 * 8, NULL, 5, NULL);
  }
}

/* Stack Size for the connection check task*/
#define STACK_SIZE 1300

/* Structure that will hold the TCB of the task being created. */
static StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
static StackType_t xStack[STACK_SIZE];

void create_ota_scheduler_task() {
  initialize_ota_updater();
  // Static task without dynamic memory allocation
  xTaskCreateStatic(
      ota_scheduler_task, "OTAScheduler", /* Task Name */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* No Parameter */
      tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure. */
}
