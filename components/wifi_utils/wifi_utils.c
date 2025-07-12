#include "wifi_utils.h"

#include "esp_bit_defs.h"

#include "configuration.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <sys/time.h>
#include <time.h>

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi";

static int s_retry_num = 0; //< Number of current connection attempts.

/**
 * @brief Event handler for WIFI_EVENT and IP_EVENT
 *
 * Handles event for wifi and ip events.
 * If wifi disconnects the handler tries to reconnect. If a connection is
 * established the retry counter is set to 0 and the IP logged.
 *
 * @param arg unused event handler arguments (NULL)
 * @param event_base Event base type (WIFI_EVENT or IP_EVENT)
 * @param event_id id of the specific event
 * @param event_data data corresponding to the event
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (CONFIG_WIFI_MAXIMUM_RETRY < 0 ||
        s_retry_num < CONFIG_WIFI_MAXIMUM_RETRY) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGD(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGW(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifi_utils_init(void) {
  // Configure and initialize wifi
  ESP_ERROR_CHECK(esp_netif_init());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Configure event handler for connect and disconnect
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  // Start Wifi connection
  wifi_config_t wifi_config = {
      .sta =
          {
              .ssid = CONFIG_WIFI_SSID,
              .password = CONFIG_WIFI_PASSWORD,
          },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
  ESP_LOGD(TAG, "wifi_init_sta finished.");
  s_wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK_WITHOUT_ABORT(wifi_utils_connect_wifi_blocking());
}

void wifi_utils_connect() {
  // Reset the retry counter
  xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
  s_retry_num = 0;
  ESP_ERROR_CHECK(esp_wifi_start());
}

esp_err_t wifi_utils_connect_wifi_blocking() {
  wifi_utils_connect();
  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
   * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
   * bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE, pdFALSE, portMAX_DELAY);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we
   * can test which event actually happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_WIFI_SSID,
             CONFIG_WIFI_PASSWORD);
    return ESP_OK;
  }

  if (bits & WIFI_FAIL_BIT) {
    ESP_LOGW(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_WIFI_SSID,
             CONFIG_WIFI_PASSWORD);
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
  }

  return ESP_FAIL;
}

void wifi_utils_check_connection_task(void *pvParameters) {
  for (;;) {
    ESP_LOGD(TAG, "Stack high water mark %d",
             uxTaskGetStackHighWaterMark(NULL));
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_FAIL_BIT) {
      esp_wifi_stop();
      ESP_LOGI(TAG, "Wifi connection failed. Wait before retrying.");
      vTaskDelay(pdMS_TO_TICKS(CONFIG_WIFI_WAIT_TIME_BETWEEN_RETRY_MS));
      wifi_utils_connect();
    }
  }
}

void wifi_utils_init_sntp(void) {
  setenv("TZ", CONFIG_LOCAL_TIME_ZONE, 1);
  tzset();

  struct tm timeinfo = {
      .tm_sec = 0,
      .tm_min = 0,
      .tm_hour = 0,
      .tm_mday = 1,
      .tm_mon = 1,
      .tm_year = 2025,
  };
  // initialize time as 10 min before the first pump cycle.
  // just in case the time is not set over SNTP we start pumping soon.
  const unsigned short nr_pump_cycles =
      configuration.pump_cycles.nr_pump_cycles;
  if (nr_pump_cycles > 0) {
    const int first_pump_time_minutes_per_day =
        ((24 * 60) +
         (configuration.pump_cycles.times_minutes_per_day[0] - 10)) %
        (24 * 60);
    const int hours = first_pump_time_minutes_per_day / 60;
    const int minutes = first_pump_time_minutes_per_day % 60;
    timeinfo.tm_hour = hours;
    timeinfo.tm_min = minutes;
  }

  struct timeval initial_time = {.tv_sec = mktime(&timeinfo), .tv_usec = 0};
  settimeofday(&initial_time, NULL);

  esp_sntp_config_t config =
      ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_WIFI_SNTP_POOL_SERVER);
  esp_netif_sntp_init(&config);
  while (esp_netif_sntp_sync_wait(CONFIG_WIFI_SNTP_INIT_WAIT_TIME_MS /
                                  portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT) {
    ESP_LOGW(TAG, "Could not set time over SNTP. Tried for %d ms",
             CONFIG_WIFI_SNTP_INIT_WAIT_TIME_MS);
    return;
  }
  ESP_LOGD(TAG, "System time synced.");
}

esp_err_t wifi_utils_get_connection_strength(int *rssi_level) {
  return esp_wifi_sta_get_rssi(rssi_level);
}

/* Stack Size for the connection check task*/
#define STACK_SIZE 1500

/* Structure that will hold the TCB of the task being created. */
static StaticTask_t xTaskBuffer;

/* Buffer that the task being created will use as its stack. Note this is
   an array of StackType_t variables. The size of StackType_t is dependent on
   the RTOS port. */
static StackType_t xStack[STACK_SIZE];

TaskHandle_t wifi_utils_create_connection_checker_task() {
  // Static task without dynamic memory allocation
  TaskHandle_t task_handle = xTaskCreateStatic(
      wifi_utils_check_connection_task, "ConnectionChecker", /* Task Name */
      STACK_SIZE,           /* Number of indexes in the xStack array. */
      NULL,                 /* No Parameter */
      tskIDLE_PRIORITY + 1, /* Priority at which the task is created. */
      xStack,               /* Array to use as the task's stack. */
      &xTaskBuffer);        /* Variable to hold the task's data structure.
                             */
  return task_handle;
}
