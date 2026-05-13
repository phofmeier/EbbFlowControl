#include "ota_scheduler.h"

#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <time.h>

#include "ota_updater.h"

static const char *TAG = "ota_scheduler";

void ota_scheduler_task(void *pvParameter) {
  for (;;) {
    setenv("TZ", CONFIG_LOCAL_TIME_ZONE, 1);
    tzset();
    time_t now = 0;
    time(&now);
    struct tm deadline;
    localtime_r(&now, &deadline);
    deadline.tm_hour = 0;
    deadline.tm_min = 0;
    deadline.tm_sec = 0;
    deadline.tm_isdst = -1;
    time_t next_midnight = mktime(&deadline);
    if (next_midnight == (time_t)-1) {
      ESP_LOGW(TAG, "mktime failed, retrying in 1 h");
      vTaskDelay(pdMS_TO_TICKS(3600 * 1000));
      continue;
    }
    double wait_sec = difftime(next_midnight, now);
    if (wait_sec <= 60) {
      deadline.tm_mday += 1;
      deadline.tm_isdst = -1;
      next_midnight = mktime(&deadline);
      if (next_midnight == (time_t)-1) {
        ESP_LOGW(TAG, "mktime failed after day roll, retrying in 1 h");
        vTaskDelay(pdMS_TO_TICKS(3600 * 1000));
        continue;
      }
      wait_sec = difftime(next_midnight, now);
    }
    const uint32_t jitter_s = esp_random() % 3601u;
    wait_sec += (double)jitter_s;
    if (wait_sec < 60.0) {
      wait_sec = 60.0;
    }

    ESP_LOGD(TAG, "Stack high water mark %d",
             uxTaskGetStackHighWaterMark(NULL));
    ESP_LOGI(TAG, "Wait %.0f seconds (incl. jitter) before next OTA window.",
             wait_sec);

    const uint64_t wait_ticks =
        (uint64_t)(wait_sec * (double)configTICK_RATE_HZ);
    const TickType_t chunk_max = pdMS_TO_TICKS(12 * 3600 * 1000ULL);
    uint64_t remaining = wait_ticks;
    while (remaining > 0) {
      TickType_t step =
          (remaining > (uint64_t)chunk_max) ? chunk_max : (TickType_t)remaining;
      if (step == 0) {
        step = 1;
      }
      vTaskDelay(step);
      remaining -= step;
    }

    xTaskCreate(ota_updater_task, "ota_updater_task", 1024 * 8, NULL, 5, NULL);
  }
}

/* Stack for the OTA scheduler task */
#define STACK_SIZE 1300

/* Structure that will hold the TCB of the task. */
static StaticTask_t xTaskBuffer;

/* Buffer that the task will use as its stack. */
static StackType_t xStack[STACK_SIZE];

void create_ota_scheduler_task() {
  initialize_ota_updater();
  xTaskCreateStatic(ota_scheduler_task, "OTAScheduler", STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, xStack, &xTaskBuffer);
}
