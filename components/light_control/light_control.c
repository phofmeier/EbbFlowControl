#include "light_control.h"

#include "configuration.h"
#include "data_logging.h"
#include "driver_gp8211s.h"
#include <esp_log.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

// #define TESTING_LIGHT_CONTROL
#define MIN_LOG_INTERVAL_S 10 * 60
#define SET_LIGHT_INTENSITY_INTERVAL_S 10

static const char *TAG = "light_control";

uint16_t last_light_intensity = 0;
time_t last_light_log_time = 0;

/**
 * @brief Get the time of the day.
 *
 * The configured local timezone is used. The number represents the minutes
 * since the start of the day.
 */
void get_cur_time(struct tm *timeinfo) {
  // set timezone
  setenv("TZ", CONFIG_LOCAL_TIME_ZONE, 1);
  tzset();

  // get local time
  time_t now;
  time(&now);
  localtime_r(&now, timeinfo);
}

void set_light_intensity(uint16_t intensity, const int force_log) {
  gp8211s_set_output(intensity);
  // only log if changed and at most every 10 minutes
  time_t now;
  time(&now);
  if (force_log || (intensity != last_light_intensity &&
                    (now - last_light_log_time) > MIN_LOG_INTERVAL_S)) {
    last_light_intensity = intensity;
    time(&last_light_log_time);
    add_light_data_item(intensity);
  }
}

void initialize_light_control() {
  ESP_LOGI(TAG, "Initializing light control");
  gp8211s_init_i2c();
  gp8211s_init_device();
  gp8211s_set_output(0);
}

void light_test() {
  // Example: Ramp up light intensity from 0 to max over 10 seconds
  for (uint16_t value = 0; value <= 0x7FFF; value += 0x0100) {
    ESP_LOGI(TAG, "Setting light intensity to %u", value);
    set_light_intensity(value, 1);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second between steps
  }
  // Hold at max intensity for 10 seconds
  vTaskDelay(pdMS_TO_TICKS(10000));
  // Ramp down light intensity from max to 0 over 10 seconds
  for (int16_t value = 0x7FFF; value >= 0; value -= 0x0100) {
    ESP_LOGI(TAG, "Setting light intensity to %u", value);
    set_light_intensity(value, 1);
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay 1 second between steps
  }
  // Hold at off for 10 seconds
  vTaskDelay(pdMS_TO_TICKS(10000));
}

void light_control_task(void *pvParameters) {
  set_light_intensity(0, 1); // Start with light off
  int needs_reconfiguration = 1;
  uint8_t currently_used_index = 0;

  while (1) {
#if defined(TESTING_LIGHT_CONTROL)
    light_test();
    continue;
#endif // TESTING_LIGHT_CONTROL

    struct tm timeinfo;
    get_cur_time(&timeinfo);
    const uint16_t current_min = timeinfo.tm_hour * 60 + timeinfo.tm_min;

    if (needs_reconfiguration) {
      needs_reconfiguration = 0;
      currently_used_index = 0;
      // Initialize light based on last run configuration
      for (size_t i = 0; i < configuration.light.nr_light_changes; i++) {
        if (configuration.light.times_min_per_day[i] <= current_min) {
          currently_used_index = i;
        }
      }
      ESP_LOGI(TAG, "Initialize light control with index %i",
               currently_used_index);
    }

    if (configuration.light.nr_light_changes < 2) {
      // Trivial configuration. Set and wait for reconfiguration.
      if (configuration.light.nr_light_changes == 1) {
        set_light_intensity(configuration.light.intensity[0], 1);
      } else {
        set_light_intensity(0, 1);
      }
      ESP_LOGI(TAG, "Waiting for reconfiguration");
      needs_reconfiguration = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      continue;
    }

    ESP_LOGD(TAG,
             "Current_min: %u, currently_used_index: %u, change_time: %u, "
             "rise_time: %u",
             current_min, currently_used_index,
             configuration.light.times_min_per_day[currently_used_index],
             configuration.light.rise_time_min[currently_used_index]);

    if (current_min >=
            configuration.light.times_min_per_day[currently_used_index] &&
        configuration.light.times_min_per_day[currently_used_index] +
                configuration.light.rise_time_min[currently_used_index] >
            current_min) {
      // We are currently changing to the next intensity level. Calculate the
      // intensity level at the current timepoint.
      const uint8_t last_index =
          (currently_used_index + configuration.light.nr_light_changes - 1) %
          configuration.light.nr_light_changes;
      const uint16_t last_intensity = configuration.light.intensity[last_index];
      const uint16_t next_intensity =
          configuration.light.intensity[currently_used_index];

      const double seconds_since_last_change =
          (current_min -
           configuration.light.times_min_per_day[currently_used_index]) *
              60 +
          timeinfo.tm_sec;
      double percent_rise_time_passed = 1.0;
      if (configuration.light.rise_time_min[currently_used_index] > 0) {
        percent_rise_time_passed =
            seconds_since_last_change /
            (configuration.light.rise_time_min[currently_used_index] * 60.0);
      }
      if (percent_rise_time_passed > 1.0) {
        percent_rise_time_passed = 1.0;
      }

      double output_intensity =
          last_intensity + percent_rise_time_passed * ((double)next_intensity -
                                                       (double)last_intensity);
      if (output_intensity < 0) {
        output_intensity = 0;
      }
      ESP_LOGD(TAG,
               "Changing light intensity from %u to %u. Percent rise time "
               "passed: %.2f%%, output intensity: %.2f",
               last_intensity, next_intensity, percent_rise_time_passed * 100.0,
               output_intensity);

      set_light_intensity((uint16_t)output_intensity, 0);

      // wait for next check
      needs_reconfiguration = ulTaskNotifyTake(
          pdTRUE, pdMS_TO_TICKS(SET_LIGHT_INTENSITY_INTERVAL_S * 1e3));
    } else {
      // wait for next change
      if (configuration.light.times_min_per_day[currently_used_index] <=
              current_min // Current timepoint is in the past. Move to next
          &&
          (currently_used_index != 0 // Special case for first index

           // If we are now smaller than the last change time we wrapped
           // around midnight. Now we are allowed to move to the next
           // index.
           || current_min < configuration.light.times_min_per_day
                                [configuration.light.nr_light_changes - 1])) {
        // set intensity to current level
        set_light_intensity(configuration.light.intensity[currently_used_index],
                            1);
        currently_used_index =
            (currently_used_index + 1) % configuration.light.nr_light_changes;
      }

      double wait_time_s =
          (configuration.light.times_min_per_day[currently_used_index] -
           current_min) *
          60;
      if (wait_time_s < 0) {
        // Wrap around midnight
        wait_time_s += 24.0 * 60.0 * 60.0;
      }
      // Wake up a little bit earlier
      wait_time_s = wait_time_s * 0.8;

      ESP_LOGI(TAG, "Wait for next change for %.2f s, index: %i", wait_time_s,
               currently_used_index);
      needs_reconfiguration = ulTaskNotifyTake(
          pdTRUE, pdMS_TO_TICKS(floor(wait_time_s) * 1e3 + 100));
    }
  }
}

TaskHandle_t create_light_control_task() {
  TaskHandle_t task_handle;
  xTaskCreate(light_control_task, "light_control_task", 4096, NULL,
              tskIDLE_PRIORITY + 2, &task_handle);
  return task_handle;
}
