#include "ota_updater.h"
#include <stdio.h>

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#ifdef CONFIG_OTA_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

static const char *TAG = "ota_updater";

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");

struct application_version_t {
  int major;
  int minor;
  int patch;
  int build;
  bool dirty;
  char description[9];
};

void log_application_version(const char *prefix,
                             const struct application_version_t *app_version) {
  if (app_version == NULL) {
    return;
  }
  ESP_LOGI(TAG, "%s Version: %d.%d.%d", prefix, app_version->major,
           app_version->minor, app_version->patch);
  if (app_version->build > 0) {
    ESP_LOGI(TAG, "Build: %d", app_version->build);
  }
  if (app_version->description[0] != '\0') {
    ESP_LOGI(TAG, "Description: %s", app_version->description);
  }
  if (app_version->dirty) {
    ESP_LOGI(TAG, "Source tree is dirty");
  }
}

/***
 * @brief Extract application version information from version string
 *
 * Assumes version string is in the format:
 * v<major>.<minor>.<patch>[-<build>-<description>][-dirty]
 * Examples: v1.2.3
 * v1.2.3-4-alpha
 * v1.2.3-dirty
 * v1.2.3-4-alpha-dirty
 *
 * @param app_version_str Version string to parse
 * @param app_version Pointer to application_version_t struct to fill
 *
 */
void extract_application_version(const char *app_version_str,
                                 struct application_version_t *app_version) {
  if (app_version_str == NULL || app_version == NULL) {
    return;
  }

  int major = 0;
  int minor = 0;
  int patch = 0;
  sscanf(app_version_str, "v%d.%d.%d", &major, &minor, &patch);

  bool dirty = false;
  if (strstr(app_version_str, "-dirty") != NULL) {
    dirty = true;
  }

  int build = 0;
  char desc[9] = "";
  const char *first_dash = strchr(app_version_str, '-');
  if (first_dash != NULL && first_dash[1] != '\0' && first_dash[1] != 'd') {
    sscanf(first_dash + 1, "%d-%8s", &build, desc);
  }

  app_version->major = major;
  app_version->minor = minor;
  app_version->patch = patch;
  app_version->dirty = dirty;
  app_version->build = build;
  snprintf(app_version->description, sizeof(app_version->description), "%s",
           desc);
}

/***
 * @brief Compare two application versions.
 *
 * Dirty versions are considered newer (bigger) than clean versions.
 * If both versions are dirty, the first version is considered newer.
 *
 * @param v1 First application version
 * @param v2 Second application version
 * @return int 1 if v1 > v2, -1 if v1 < v2, 0 if equal or invalid
 */
int compare_application_versions(const struct application_version_t *v1,
                                 const struct application_version_t *v2) {
  if (v1 == NULL || v2 == NULL) {
    return 0;
  }

  if (v1->major != v2->major) {
    return (v1->major > v2->major) ? 1 : -1;
  }
  if (v1->minor != v2->minor) {
    return (v1->minor > v2->minor) ? 1 : -1;
  }
  if (v1->patch != v2->patch) {
    return (v1->patch > v2->patch) ? 1 : -1;
  }
  if (v1->build != v2->build) {
    return (v1->build > v2->build) ? 1 : -1;
  }
  // Assume dirty version is newer than clean version
  // if both are dirty the first version is considered newer
  if (v1->dirty != v2->dirty) {
    return (v1->dirty) ? 1 : -1;
  }
  if (v1->dirty) {
    return 1;
  }

  return 0;
}

/* Event handler for catching system events */
static void ota_updater_event_handler(void *arg, esp_event_base_t event_base,
                                      int32_t event_id, void *event_data) {
  if (event_base == ESP_HTTPS_OTA_EVENT) {
    switch (event_id) {
    case ESP_HTTPS_OTA_START:
      ESP_LOGI(TAG, "OTA started");
      break;
    case ESP_HTTPS_OTA_CONNECTED:
      ESP_LOGI(TAG, "Connected to server");
      break;
    case ESP_HTTPS_OTA_GET_IMG_DESC:
      ESP_LOGI(TAG, "Reading Image Description");
      break;
    case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
      ESP_LOGI(TAG, "Verifying chip id of new image: %d",
               *(esp_chip_id_t *)event_data);
      break;
    case ESP_HTTPS_OTA_VERIFY_CHIP_REVISION:
      ESP_LOGI(TAG, "Verifying chip revision of new image: %d",
               *(esp_chip_id_t *)event_data);
      break;
    case ESP_HTTPS_OTA_DECRYPT_CB:
      ESP_LOGI(TAG, "Callback to decrypt function");
      break;
    case ESP_HTTPS_OTA_WRITE_FLASH:
      ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
      break;
    case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
      ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d",
               *(esp_partition_subtype_t *)event_data);
      break;
    case ESP_HTTPS_OTA_FINISH:
      ESP_LOGI(TAG, "OTA finish");
      break;
    case ESP_HTTPS_OTA_ABORT:
      ESP_LOGI(TAG, "OTA abort");
      break;
    }
  }
}

/***
 * @brief Validate the new image header before proceeding with OTA
 */
static esp_err_t validate_image_header(const esp_app_desc_t *new_app_info) {
  if (new_app_info == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_app_desc_t running_app_info;
  struct application_version_t running_app_version;

  if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
    extract_application_version(running_app_info.version, &running_app_version);
    log_application_version("Running", &running_app_version);
  }

  struct application_version_t new_app_version;
  extract_application_version(new_app_info->version, &new_app_version);
  log_application_version("New", &new_app_version);

  // Allow update if factory app is running
  if (strncmp(running_app_info.project_name, "EbbFlowControl-factory",
              sizeof(running_app_info.project_name)) == 0) {
    ESP_LOGI(TAG, "Factory app is running, allowing update to proceed.");
    return ESP_OK;
  }
  const int cmp_result =
      compare_application_versions(&new_app_version, &running_app_version);
  if (cmp_result < 0) {
    ESP_LOGW(TAG, "New version is older than the running version. We will not "
                  "continue the update.");
    return ESP_FAIL;
  } else if (cmp_result == 0) {
    ESP_LOGW(TAG, "New version is the same as the running version. We will not "
                  "continue the update.");
    return ESP_FAIL;
  }

  return ESP_OK;
}

void initialize_ota_updater() {
  ESP_LOGI(TAG, "OTA Updater initialized");
  ESP_ERROR_CHECK(esp_event_handler_register(
      ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_updater_event_handler, NULL));
}

void ota_updater_task(void *pvParameter) {
#ifndef CONFIG_OTA_USE_CERT_BUNDLE
  ESP_LOGD(TAG, "Server Certificate: \n%s", server_cert_pem_start);
#endif
  ESP_LOGD(TAG, "Connecting to %s", CONFIG_OTA_FIRMWARE_UPGRADE_URL);

  esp_http_client_config_t config = {
      .url = CONFIG_OTA_FIRMWARE_UPGRADE_URL,
#ifdef CONFIG_OTA_USE_CERT_BUNDLE
      .crt_bundle_attach = esp_crt_bundle_attach,
#else
      .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_OTA_USE_CERT_BUNDLE */
      .keep_alive_enable = true,
      .buffer_size_tx = 1024,
  };

  esp_https_ota_config_t ota_config = {
      .http_config = &config,
  };

  esp_https_ota_handle_t https_ota_handle = NULL;
  esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
    vTaskDelete(NULL);
  }

  esp_app_desc_t app_desc = {};
  err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_https_ota_get_img_desc failed");
    goto ota_end;
  }
  err = validate_image_header(&app_desc);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "image header verification failed");
    goto ota_end;
  }

  while (1) {
    err = esp_https_ota_perform(https_ota_handle);
    if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
      break;
    }
    // esp_https_ota_perform returns after every read operation which gives
    // user the ability to monitor the status of OTA upgrade by calling
    // esp_https_ota_get_image_len_read, which gives length of image data read
    // so far.
    const size_t len = esp_https_ota_get_image_len_read(https_ota_handle);
    ESP_LOGD(TAG, "Image bytes read: %d", len);
  }

  if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
    // the OTA image was not completely received and user can customise the
    // response to this situation.
    ESP_LOGE(TAG, "Complete data was not received.");
  } else {
    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
      ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      esp_restart();
    } else {
      if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
        ESP_LOGE(TAG, "Image validation failed, image is corrupted");
      }
      ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
      vTaskDelete(NULL);
    }
  }

ota_end:
  esp_https_ota_abort(https_ota_handle);
  ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
  vTaskDelete(NULL);
}

void mark_running_app_version_valid() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
      if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
        ESP_LOGI(TAG, "App is valid, rollback cancelled successfully");
      } else {
        ESP_LOGE(TAG, "Failed to cancel rollback");
      }
    }
  }
}
