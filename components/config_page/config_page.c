#include "config_page.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configuration.h"

extern const char config_page_start[] asm("_binary_config_page_html_start");
extern const char config_page_end[] asm("_binary_config_page_html_end");

static const char *TAG = "config_page";
TaskHandle_t configuration_task_handle_ = NULL;

/** URL decode a string in place */
void urldecode2(char *dst, const char *src) {
  while (*src) {
    char a, b;
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
        (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a')
        a -= 'a' - 'A';
      if (a >= 'A')
        a -= ('A' - 10);
      else
        a -= '0';
      if (b >= 'a')
        b -= 'a' - 'A';
      if (b >= 'A')
        b -= ('A' - 10);
      else
        b -= '0';
      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
  *dst++ = '\0';
}

// Helper function to replace placeholder in HTML
static void replace_placeholder(char *html, const char *placeholder,
                                const char *value) {
  char *pos = strstr(html, placeholder);
  if (pos) {
    size_t placeholder_len = strlen(placeholder);
    size_t value_len = strlen(value);
    memmove(pos + value_len, pos + placeholder_len,
            strlen(pos + placeholder_len) + 1);
    memcpy(pos, value, value_len);
  }
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req) {
  // Estimated length for config values
  const u_int32_t configuration_length =
      3 + // Board ID length
      strlen(configuration.network.ssid) +
      strlen(configuration.network.password) + //
      40 +                                     // Wifi status length
      strlen(configuration.network.mqtt_broker) +
      strlen(configuration.network.mqtt_username) +
      strlen(configuration.network.mqtt_password) + //
      40;                                           // MQTT status length
  const uint32_t root_len =
      config_page_end - config_page_start; // cppcheck-suppress comparePointers
  char *html = malloc(root_len + configuration_length + 1);
  if (!html) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Memory allocation failed");
    return ESP_FAIL;
  }
  memcpy(html, config_page_start, root_len);
  html[root_len] = '\0';

  // Replace placeholders with configuration values
  char board_id_str[4];
  sprintf(board_id_str, "%d", configuration.id);
  replace_placeholder(html, "{{board_id}}", board_id_str);
  replace_placeholder(html, "{{ssid}}", configuration.network.ssid);
  replace_placeholder(html, "{{wifi_password}}",
                      configuration.network.password);

  const char *wifi_status =
      (configuration.network.valid_bits & NETWORK_WIFI_VALID_BIT)
          ? "<span style='color: green;'>OK</span>"
          : "<span style='color: red;'>FAILED</span>";
  replace_placeholder(html, "{{wifi_status}}", wifi_status);

  replace_placeholder(html, "{{mqtt}}", configuration.network.mqtt_broker);
  replace_placeholder(html, "{{mqtt_username}}",
                      configuration.network.mqtt_username);
  replace_placeholder(html, "{{mqtt_password}}",
                      configuration.network.mqtt_password);
  const char *mqtt_status =
      (configuration.network.valid_bits & NETWORK_MQTT_VALID_BIT)
          ? "<span style='color: green;'>OK</span>"
          : "<span style='color: red;'>FAILED</span>";
  replace_placeholder(html, "{{mqtt_status}}", mqtt_status);

  ESP_LOGI(TAG, "Serve root");
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, html, strlen(html));

  free(html);
  return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/", .method = HTTP_GET, .handler = root_get_handler};

static esp_err_t set_config_post_handler(httpd_req_t *req) {
  char buf[1024];
  int ret, remaining = req->content_len;

  if (remaining > sizeof(buf) - 1) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
    return ESP_FAIL;
  }

  ret = httpd_req_recv(req, buf, remaining);
  if (ret <= 0) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Failed to receive data");
    return ESP_FAIL;
  }
  buf[ret] = '\0';

  ESP_LOGI(TAG, "Received POST data: %s", buf);

  // Parse the form data
  char *token = strtok(buf, "&");
  while (token != NULL) {
    const char *key = token;
    char *value = strchr(token, '=');
    if (value) {
      *value = '\0';
      value++;
      // URL decode value if needed, but for simplicity assume no special chars
      if (strcmp(key, "board_id") == 0) {
        configuration.id = atoi(value);
      } else if (strcmp(key, "ssid") == 0) {
        urldecode2(configuration.network.ssid, value);
      } else if (strcmp(key, "wifi_password") == 0) {
        urldecode2(configuration.network.password, value);
      } else if (strcmp(key, "mqtt") == 0) {
        urldecode2(configuration.network.mqtt_broker, value);
      } else if (strcmp(key, "mqtt_username") == 0) {
        urldecode2(configuration.network.mqtt_username, value);
      } else if (strcmp(key, "mqtt_password") == 0) {
        urldecode2(configuration.network.mqtt_password, value);
      }
    }
    token = strtok(NULL, "&");
  }

  // Save the configuration
  save_configuration();

  // Respond with success
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req,
                  "<html><body>Configuration updated successfully. <a "
                  "href='/'>Back</a></body></html>",
                  -1);
  xTaskNotifyGive(configuration_task_handle_);

  return ESP_OK;
}

static const httpd_uri_t set_config = {.uri = "/setConfig",
                                       .method = HTTP_POST,
                                       .handler = set_config_post_handler};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
  // Set status
  httpd_resp_set_status(req, "303 See Other");
  // Redirect to the "/" root directory
  httpd_resp_set_hdr(req, "Location", "/");
  // iOS requires content in the response to detect a captive portal, simply
  // redirecting is not sufficient.
  httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

  ESP_LOGI(TAG, "Redirecting to root");
  return ESP_OK;
}

static httpd_handle_t start_webserver(void) {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets = 5;
  config.lru_purge_enable = true;

  // Start the httpd server
  ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
  if (httpd_start(&server, &config) == ESP_OK) {
    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &set_config);
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND,
                               http_404_error_handler);
  }
  return server;
}

void serve_config_page(TaskHandle_t configuration_task_handle) {
  configuration_task_handle_ = configuration_task_handle;
  /*
      Turn of warnings from HTTP server as redirecting traffic will yield
      lots of invalid requests
  */
  esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
  esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
  esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

  //   // Initialize networking stack
  //   ESP_ERROR_CHECK(esp_netif_init());

  //   // Create default event loop needed by the  main app
  //   ESP_ERROR_CHECK(esp_event_loop_create_default());

  //   // Initialize NVS needed by Wi-Fi
  //   ESP_ERROR_CHECK(nvs_flash_init());

  //   // Initialise ESP32 in SoftAP mode
  //   wifi_init_softap();

  //   // Configure DNS-based captive portal, if configured
  //   dhcp_set_captiveportal_url();

  // Start the server for the first time
  start_webserver();
}
