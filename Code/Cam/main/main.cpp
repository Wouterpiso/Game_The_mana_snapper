#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace {
constexpr uart_port_t CamUart = UART_NUM_1;
// The U0T pad of the ESP32-CAM header.
constexpr int CamTxPin = 1;
constexpr int BaudRate = 921600;
constexpr char Tag[] = "Cam";
constexpr uint8_t FrameMagic[4] = {0xA5, 0x5A, 0xE7, 0xC3};
// Small, low quality QVGA frames keep the preview smooth over the 921600 baud
// link; the scan demo shows this same frame as its photo.
constexpr uint32_t FrameIntervalMs = 40;
constexpr int PreviewQuality = 22;

// AI-Thinker ESP32-CAM module pin map.
camera_config_t makeCameraConfig()
{
    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_pwdn = 32;
    config.pin_reset = -1;
    config.pin_xclk = 0;
    config.pin_sccb_sda = 26;
    config.pin_sccb_scl = 27;
    config.pin_d7 = 35;
    config.pin_d6 = 34;
    config.pin_d5 = 39;
    config.pin_d4 = 36;
    config.pin_d3 = 21;
    config.pin_d2 = 19;
    config.pin_d1 = 18;
    config.pin_d0 = 5;
    config.pin_vsync = 25;
    config.pin_href = 23;
    config.pin_pclk = 22;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    // The driver sizes its frame buffers from this resolution, so it starts at
    // the largest size used (photo frames) and previews switch down from here.
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = PreviewQuality;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
    return config;
}

void uartStreamInit()
{
    const uart_config_t config = {
        .baud_rate = BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(CamUart, &config));
    ESP_ERROR_CHECK(uart_set_pin(CamUart, CamTxPin, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(CamUart, 1024, 32 * 1024, 0, nullptr, 0));
}

void sendFrame(const uint8_t* data, size_t length)
{
    uint8_t header[8];
    const uint32_t size = static_cast<uint32_t>(length);
    std::memcpy(header, FrameMagic, sizeof(FrameMagic));
    header[4] = static_cast<uint8_t>(size & 0xFF);
    header[5] = static_cast<uint8_t>((size >> 8) & 0xFF);
    header[6] = static_cast<uint8_t>((size >> 16) & 0xFF);
    header[7] = static_cast<uint8_t>((size >> 24) & 0xFF);
    uart_write_bytes(CamUart, header, sizeof(header));
    uart_write_bytes(CamUart, data, length);
    uart_wait_tx_done(CamUart, pdMS_TO_TICKS(5000));
}

void streamPreview()
{
    camera_fb_t* frame = esp_camera_fb_get();
    if (frame != nullptr) {
        sendFrame(frame->buf, frame->len);
        esp_camera_fb_return(frame);
    }
}
}

extern "C" void app_main(void)
{
    camera_config_t config = makeCameraConfig();
    esp_err_t error = esp_camera_init(&config);
    if (error != ESP_OK) {
        // Modules without (working) PSRAM fall back to internal RAM; a QQVGA
        // JPEG frame fits there easily.
        ESP_LOGW(Tag, "Camera init failed (0x%x), retrying without PSRAM", error);
        esp_camera_deinit();
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.fb_count = 1;
        error = esp_camera_init(&config);
    }
    if (error != ESP_OK) {
        ESP_LOGE(Tag, "Camera init failed (0x%x)", error);
        return;
    }
    ESP_LOGI(Tag, "Camera ready, streaming to the U0T pad");
    // UART1 is routed over GPIO1, which hides the console output from here on.
    esp_log_level_set(Tag, ESP_LOG_NONE);

    sensor_t* sensor = esp_camera_sensor_get();
    if (sensor != nullptr) {
        // Set once: reprogramming the sensor on every frame costs time.
        sensor->set_framesize(sensor, FRAMESIZE_QVGA);
        sensor->set_quality(sensor, PreviewQuality);
    }
    uartStreamInit();

    while (true) {
        streamPreview();
        vTaskDelay(pdMS_TO_TICKS(FrameIntervalMs));
    }
}
