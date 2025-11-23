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
  if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
    ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
  }

  // Allow update if factory app is running
  if (strncmp(running_app_info.project_name, "EbbFlowControl-factory",
              sizeof(running_app_info.project_name)) == 0) {
    ESP_LOGI(TAG, "Factory app is running, allowing update to proceed.");
    return ESP_OK;
  }

  if (memcmp(new_app_info->version, running_app_info.version,
             sizeof(new_app_info->version)) == 0) {
    ESP_LOGW(TAG, "Current running version is the same as a new. We will not "
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
  ESP_LOGI(TAG, "Starting OTA example task");
  esp_http_client_config_t config = {
      .url = CONFIG_OTA_FIRMWARE_UPGRADE_URL,
#ifdef CONFIG_OTA_USE_CERT_BUNDLE
      .crt_bundle_attach = esp_crt_bundle_attach,
#else
      .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_OTA_USE_CERT_BUNDLE */
      .keep_alive_enable = true,
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
    // esp_https_ota_perform returns after every read operation which gives user
    // the ability to monitor the status of OTA upgrade by calling
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
