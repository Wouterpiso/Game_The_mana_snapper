#include "ImageReconition.hpp"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "cJSON.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr char Tag[] = "ImageRecognition";
constexpr char ServerUrl[] = "http://192.168.1.1:5000/ocr";
constexpr char MultipartBoundary[] = "ManaSnapperBoundary";

void wifiInit()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifiConfig));

    wifi_config_t config = {};
    std::snprintf(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid),
                  "%s", CONFIG_MANA_SNAPPER_WIFI_SSID);
    std::snprintf(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password),
                  "%s", CONFIG_MANA_SNAPPER_WIFI_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void printWords(const char* responseBody)
{
    cJSON* root = cJSON_Parse(responseBody);
    if (root == nullptr) {
        ESP_LOGW(Tag, "OCR returned invalid JSON");
        return;
    }

    cJSON* words = cJSON_GetObjectItem(root, "words");
    cJSON* word = nullptr;
    cJSON_ArrayForEach(word, words) {
        if (cJSON_IsString(word)) {
            ESP_LOGI(Tag, "OCR word: %s", word->valuestring);
        }
    }
    cJSON_Delete(root);
}
}

void imageRecognitionInit()
{
    wifiInit();
}

bool imageRecognitionSubmit(const uint8_t* imageData, size_t imageLength)
{
    if (imageData == nullptr || imageLength == 0) {
        ESP_LOGW(Tag, "No image received");
        return false;
    }

    char contentType[96];
    std::snprintf(contentType, sizeof(contentType),
                  "multipart/form-data; boundary=%s", MultipartBoundary);

    char header[512];
    const int headerLength = std::snprintf(
        header, sizeof(header),
        "--%s\r\nContent-Disposition: form-data; name=\"image\"; filename=\"card.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n",
        MultipartBoundary);
    char footer[128];
    const int footerLength = std::snprintf(
        footer, sizeof(footer), "\r\n--%s--\r\n", MultipartBoundary);

    if (headerLength < 0 || footerLength < 0) {
        ESP_LOGE(Tag, "Failed to build multipart request");
        return false;
    }

    esp_http_client_config_t config = {};
    config.url = ServerUrl;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 30000;

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
    if (success && esp_http_client_write(client,
                                         reinterpret_cast<const char*>(imageData),
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

    char response[4096] = {};
    const int bytesRead = esp_http_client_read_response(client, response,
                                                        sizeof(response) - 1);
    if (bytesRead >= 0) {
        response[bytesRead] = '\0';
        ESP_LOGI(Tag, "OCR HTTP status: %d", esp_http_client_get_status_code(client));
        printWords(response);
    }

    const int finalStatus = esp_http_client_get_status_code(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return finalStatus == 200;
}
