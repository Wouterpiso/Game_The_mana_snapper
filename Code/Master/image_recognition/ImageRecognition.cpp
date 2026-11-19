/**
 * @file ImageRecognition.cpp
 * @brief Implements WiFi setup and card-title recognition through OCR.
 */
#include "ImageRecognition.hpp"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "cJSON.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr char Tag[] = "ImageRecognition";
constexpr char MultipartBoundary[] = "ManaSnapperBoundary";

EventGroupHandle_t wifiEvents = nullptr;
constexpr EventBits_t GotIpBit = BIT0;

/**
 * @brief Handles WiFi and IP events.
 * @param base Event base.
 * @param id Event id.
 */
void onWifiEvent(void*, esp_event_base_t base, int32_t id, void*) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(wifiEvents, GotIpBit);
    ESP_LOGW(Tag, "WiFi disconnected, retrying");
    esp_wifi_connect();
  } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(wifiEvents, GotIpBit);
  }
}

/**
 * @brief Initialises NVS, the network stack and the WiFi station.
 */
void wifiInit() {
  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifiEvents = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onWifiEvent, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onWifiEvent, nullptr));

  wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifiConfig));

  wifi_config_t config = {};
  std::snprintf(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid), "%s",
                CONFIG_MANA_SNAPPER_WIFI_SSID);
  std::snprintf(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password), "%s",
                CONFIG_MANA_SNAPPER_WIFI_PASSWORD);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
  ESP_ERROR_CHECK(esp_wifi_start());
}

/**
 * @brief Copies the first non-empty OCR line into a buffer.
 *
 * The first line of a card is its title, so that is treated as the name.
 *
 * @param responseBody JSON returned by the OCR server.
 * @param name Buffer that receives the title.
 * @param nameSize Size of @p name in bytes.
 * @return True when a title was found.
 */
bool extractCardName(const char* responseBody, char* name, size_t nameSize) {
  cJSON* root = cJSON_Parse(responseBody);
  if (root == nullptr) {
    ESP_LOGW(Tag, "OCR returned invalid JSON");
    return false;
  }

  bool found = false;
  cJSON* words = cJSON_GetObjectItem(root, "words");
  cJSON* word = nullptr;
  cJSON_ArrayForEach(word, words) {
    if (!cJSON_IsString(word) || word->valuestring == nullptr) {
      continue;
    }
    if (!found && word->valuestring[0] != '\0') {
      std::snprintf(name, nameSize, "%s", word->valuestring);
      found = true;
    }
  }
  cJSON_Delete(root);
  return found;
}
} // namespace

void imageRecognitionInit() {
  wifiInit();
}

bool imageRecognitionEnsureWifi() {
  return (xEventGroupWaitBits(wifiEvents, GotIpBit, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000)) &
          GotIpBit) != 0;
}

bool imageRecognitionGetCardName(const uint8_t* imageData, size_t imageLength, char* name,
                                 size_t nameSize) {
  if (name == nullptr || nameSize == 0) {
    return false;
  }
  name[0] = '\0';

  if (!imageRecognitionEnsureWifi()) {
    ESP_LOGE(Tag, "No WiFi connection (no IP address)");
    return false;
  }
  if (imageData == nullptr || imageLength == 0) {
    ESP_LOGW(Tag, "No image received");
    return false;
  }

  char contentType[96];
  std::snprintf(contentType, sizeof(contentType), "multipart/form-data; boundary=%s",
                MultipartBoundary);

  char header[512];
  const int headerLength = std::snprintf(
      header, sizeof(header),
      "--%s\r\nContent-Disposition: form-data; name=\"image\"; filename=\"card.jpg\"\r\n"
      "Content-Type: image/jpeg\r\n\r\n",
      MultipartBoundary);
  char footer[128];
  const int footerLength =
      std::snprintf(footer, sizeof(footer), "\r\n--%s--\r\n", MultipartBoundary);

  if (headerLength < 0 || footerLength < 0) {
    ESP_LOGE(Tag, "Failed to build multipart request");
    return false;
  }

  char serverUrl[128];
  std::snprintf(serverUrl, sizeof(serverUrl), "%s/ocr", CONFIG_MANA_SNAPPER_SERVER_URL);

  esp_http_client_config_t config = {};
  config.url = serverUrl;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = 12000;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    ESP_LOGE(Tag, "Failed to create HTTP client");
    return false;
  }

  esp_http_client_set_header(client, "Content-Type", contentType);

  const size_t contentLength =
      static_cast<size_t>(headerLength) + imageLength + static_cast<size_t>(footerLength);
  esp_http_client_set_header(client, "Accept", "application/json");

  esp_err_t err = esp_http_client_open(client, contentLength);
  if (err != ESP_OK) {
    ESP_LOGE(Tag, "Failed to open HTTP request: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return false;
  }

  bool success = true;
  if (esp_http_client_write(client, header, headerLength) != headerLength) {
    success = false;
  }
  if (success && esp_http_client_write(client, reinterpret_cast<const char*>(imageData),
                                       imageLength) != static_cast<int>(imageLength)) {
    success = false;
  }
  if (success && esp_http_client_write(client, footer, footerLength) != footerLength) {
    success = false;
  }

  if (!success) {
    ESP_LOGE(Tag, "Failed to write multipart image");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  const int status = esp_http_client_fetch_headers(client);
  if (status < 0) {
    ESP_LOGE(Tag, "OCR server returned no response");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  constexpr size_t ResponseSize = 4096;
  char* response = static_cast<char*>(std::calloc(ResponseSize, 1));
  if (response == nullptr) {
    ESP_LOGE(Tag, "Failed to allocate response buffer");
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return false;
  }

  bool gotName = false;
  const int bytesRead = esp_http_client_read_response(client, response, ResponseSize - 1);
  const int finalStatus = esp_http_client_get_status_code(client);
  if (bytesRead >= 0) {
    response[bytesRead] = '\0';
    if (finalStatus == 200) {
      gotName = extractCardName(response, name, nameSize);
    } else {
      ESP_LOGW(Tag, "OCR response: %s", response);
    }
  }

  std::free(response);
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return gotName;
}
