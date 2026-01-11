#include "wifi_utils_sntp.h"

#include "configuration.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "wifi_sntp";

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
