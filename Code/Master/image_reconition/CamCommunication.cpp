#include "CamCommunication.hpp"

#include "ImageReconition.hpp"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <cstring>

namespace {
constexpr uart_port_t CamUart = UART_NUM_2;
constexpr int CamTxPin = 25;
constexpr int CamRxPin = 26;
constexpr int BaudRate = 115200;
constexpr char Tag[] = "CamCommunication";
constexpr uint8_t FrameMagic[4] = {0xA5, 0x5A, 0xE7, 0xC3};
constexpr size_t MaxImageSize = 512 * 1024;

enum class ReceiveState {
    WaitingForMagic,
    ReadingHeader,
    ReadingImage,
};

ReceiveState receiveState = ReceiveState::WaitingForMagic;
size_t magicMatches = 0;
uint8_t lengthBytes[4];
size_t lengthBytesReceived = 0;
size_t imageLength = 0;
size_t imageBytesReceived = 0;
uint8_t* imageBuffer = nullptr;

uint32_t readLength(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

void resetReceive()
{
    receiveState = ReceiveState::WaitingForMagic;
    magicMatches = 0;
    lengthBytesReceived = 0;
    imageLength = 0;
    imageBytesReceived = 0;
}
}

void camCommunicationInit()
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
    ESP_ERROR_CHECK(uart_set_pin(CamUart, CamTxPin, CamRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(CamUart, 2048, 2048, 0, nullptr, 0));
}

bool camCommunicationHasData()
{
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(CamUart, &available));
    return available > 0;
}

void camCommunicationHandle()
{
    uint8_t data[512];
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(CamUart, &available));

    const size_t bytesToRead = available < sizeof(data) ? available : sizeof(data);
    if (bytesToRead == 0) {
        return;
    }

    const int bytesRead = uart_read_bytes(CamUart, data, bytesToRead, 0);
    if (bytesRead <= 0) {
        return;
    }

    for (int index = 0; index < bytesRead; ++index) {
        const uint8_t byte = data[index];

        if (receiveState == ReceiveState::WaitingForMagic) {
            if (byte == FrameMagic[magicMatches]) {
                ++magicMatches;
                if (magicMatches == sizeof(FrameMagic)) {
                    receiveState = ReceiveState::ReadingHeader;
                    magicMatches = 0;
                    lengthBytesReceived = 0;
                }
            } else {
                magicMatches = (byte == FrameMagic[0]) ? 1 : 0;
            }
            continue;
        }

        if (receiveState == ReceiveState::ReadingHeader) {
            lengthBytes[lengthBytesReceived++] = byte;
            if (lengthBytesReceived == sizeof(lengthBytes)) {
                imageLength = readLength(lengthBytes);
                if (imageLength == 0 || imageLength > MaxImageSize) {
                    ESP_LOGW(Tag, "Invalid image frame length: %u", (unsigned)imageLength);
                    resetReceive();
                    continue;
                }

                imageBuffer = static_cast<uint8_t*>(heap_caps_malloc(imageLength, MALLOC_CAP_8BIT));
                if (imageBuffer == nullptr) {
                    ESP_LOGE(Tag, "Failed to allocate %u byte image buffer", (unsigned)imageLength);
                    resetReceive();
                    continue;
                }

                imageBytesReceived = 0;
                receiveState = ReceiveState::ReadingImage;
            }
            continue;
        }

        const size_t remaining = imageLength - imageBytesReceived;
        const size_t copyLength = remaining > 1 ? 1 : remaining;
        imageBuffer[imageBytesReceived] = byte;
        imageBytesReceived += copyLength;

        if (imageBytesReceived == imageLength) {
            ESP_LOGI(Tag, "Received image frame: %u bytes", (unsigned)imageLength);
            imageRecognitionSubmit(imageBuffer, imageLength);
            heap_caps_free(imageBuffer);
            imageBuffer = nullptr;
            resetReceive();
        }
    }
}