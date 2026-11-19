#include "DisplayCommunication.hpp"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdio.h>

namespace {
constexpr uart_port_t DisplayUart = UART_NUM_1;
constexpr int DisplayTxPin = 17;
constexpr int DisplayRxPin = 16;
constexpr int BaudRate = 921600;
constexpr size_t MaxTitleLength = 47;
int lastArgument = -1;
char lastTitle[MaxTitleLength + 1];

bool sendBytes(const char* data, size_t length)
{
    const int bytesWritten = uart_write_bytes(DisplayUart, data, length);
    if (bytesWritten != static_cast<int>(length)) {
        printf("Failed to send data to screen\n");
        return false;
    }
    return true;
}

DisplayRequest commandFromLine(const char* line, size_t length)
{
    if (length > 0 && line[length - 1] == '\r') {
        --length;
    }
    constexpr size_t titlePrefixLength = sizeof("ADD_CARD_TITLE ") - 1;
    if (length > titlePrefixLength &&
        std::strncmp(line, "ADD_CARD_TITLE ", titlePrefixLength) == 0) {
        size_t titleLength = length - titlePrefixLength;
        if (titleLength > MaxTitleLength) {
            titleLength = MaxTitleLength;
        }
        std::memcpy(lastTitle, line + titlePrefixLength, titleLength);
        lastTitle[titleLength] = '\0';
        return DisplayRequest::AddCardTitle;
    }
    if (length == 8 && std::strncmp(line, "OCR_DEMO", 8) == 0) {
        return DisplayRequest::OcrDemo;
    }
    if (length == 9 && std::strncmp(line, "CAM_FRAME", 9) == 0) {
        return DisplayRequest::CamFrame;
    }
    if (length == 13 && std::strncmp(line, "CAPTURE_ABORT", 13) == 0) {
        return DisplayRequest::CaptureAbort;
    }
    if (length == 7 && std::strncmp(line, "CAPTURE", 7) == 0) {
        return DisplayRequest::Capture;
    }
    if (length == 10 && std::strncmp(line, "COLLECTION", 10) == 0) {
        return DisplayRequest::Collection;
    }
    if (length == 5 && std::strncmp(line, "DECKS", 5) == 0) {
        return DisplayRequest::Display;
    }
    if (length == 6 && std::strncmp(line, "IMPORT", 6) == 0) {
        return DisplayRequest::Import;
    }
    if (length == 6 && std::strncmp(line, "EXPORT", 6) == 0) {
        return DisplayRequest::Export;
    }
    // "CARD_INFO <number>", "CARD_IMAGE <number>", "CARD_REMOVE <number>",
    // "CARD_REMOVE_ONE <number>" and "ADD_CARD_SET <number>".
    char text[32] = {};
    std::strncpy(text, line, length < sizeof(text) - 1 ? length : sizeof(text) - 1);
    if (std::strncmp(text, "ADD_CARD_SET ", 13) == 0) {
        lastArgument = std::atoi(text + 13);
        return DisplayRequest::AddCardSet;
    }
    if (std::strncmp(text, "CARD_INFO ", 10) == 0) {
        lastArgument = std::atoi(text + 10);
        return DisplayRequest::CardInfo;
    }
    if (std::strncmp(text, "CARD_IMAGE ", 11) == 0) {
        lastArgument = std::atoi(text + 11);
        return DisplayRequest::CardImage;
    }
    if (std::strncmp(text, "CARD_REMOVE_ONE ", 16) == 0) {
        lastArgument = std::atoi(text + 16);
        return DisplayRequest::CardRemoveOne;
    }
    if (std::strncmp(text, "CARD_REMOVE ", 12) == 0) {
        lastArgument = std::atoi(text + 12);
        return DisplayRequest::CardRemove;
    }
    return DisplayRequest::Idle;
}
}

void displayCommunicationInit()
{
    const uart_config_t config = {
        .baud_rate = BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(DisplayUart, &config));
    ESP_ERROR_CHECK(
        uart_set_pin(DisplayUart, DisplayTxPin, DisplayRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(DisplayUart, 2048, 0, 0, nullptr, 0));
}

bool displayCommunicationHasData()
{
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(DisplayUart, &available));
    return available > 0;
}

DisplayRequest displayCommunicationReadRequest()
{
    static char commandBuffer[256];
    static size_t commandLength = 0;
    uint8_t data[128];
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(DisplayUart, &available));

    const size_t bytesToRead =
        available < sizeof(data) - 1 ? available : sizeof(data) - 1;
    if (bytesToRead > 0) {
        const int bytesRead = uart_read_bytes(DisplayUart, data, bytesToRead, 0);
        printf("DISPLAY RX %d bytes: %.*s\n", bytesRead, bytesRead > 0 ? bytesRead : 0, (const char*)data);
        if (bytesRead < 0) {
            printf("Failed to read from screen\n");
            return DisplayRequest::Idle;
        }
        for (int index = 0; index < bytesRead; ++index) {
            if (commandLength >= sizeof(commandBuffer) - 1) {
                commandLength = 0;
            }
            commandBuffer[commandLength++] =
                static_cast<char>(data[index]);
        }
    }

    for (size_t lineEnd = 0; lineEnd < commandLength; ++lineEnd) {
        if (commandBuffer[lineEnd] != '\n') {
            continue;
        }

        const DisplayRequest request =
            commandFromLine(commandBuffer, lineEnd);
        const size_t remainingLength = commandLength - lineEnd - 1;
        std::memmove(commandBuffer, commandBuffer + lineEnd + 1,
                     remainingLength);
        commandLength = remainingLength;
        if (request != DisplayRequest::Idle) {
            return request;
        }
        lineEnd = static_cast<size_t>(-1);
    }
    return DisplayRequest::None;
}

bool displayCommunicationSendCollectionList(const char* collectionListJson)
{
    if (collectionListJson == nullptr) {
        return false;
    }

    constexpr char prefix[] = "COLLECTION_LIST ";
    constexpr char suffix[] = "\n";
    return sendBytes(prefix, sizeof(prefix) - 1) &&
           sendBytes(collectionListJson, std::strlen(collectionListJson)) &&
           sendBytes(suffix, sizeof(suffix) - 1) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendAddCardResult(bool success)
{
    const char* message = success ? "ADD_CARD_DONE\n" : "ADD_CARD_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

int displayCommunicationReadArgument()
{
    return lastArgument;
}

const char* displayCommunicationReadTitle()
{
    return lastTitle;
}

bool displayCommunicationSendAddCardSets(const char* payload)
{
    if (payload == nullptr) {
        return false;
    }
    constexpr char prefix[] = "ADD_CARD_SETS ";
    constexpr char suffix[] = "\n";
    return sendBytes(prefix, sizeof(prefix) - 1) &&
           sendBytes(payload, std::strlen(payload)) &&
           sendBytes(suffix, sizeof(suffix) - 1) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(2000)) == ESP_OK;
}

bool displayCommunicationSendOcrDemoResult(int testImage, const char* text)
{
    if (text == nullptr) {
        return false;
    }
    char header[40];
    const int headerLength =
        std::snprintf(header, sizeof(header), "OCR_DEMO_RESULT %d ", testImage);
    return sendBytes(header, static_cast<size_t>(headerLength)) &&
           sendBytes(text, std::strlen(text)) &&
           sendBytes("\n", 1) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(500)) == ESP_OK;
}

bool displayCommunicationSendOcrDemoFailed()
{
    const char* message = "OCR_DEMO_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendCardInfo(const char* cardJson)
{
    if (cardJson == nullptr) {
        return false;
    }
    constexpr char prefix[] = "CARD_INFO ";
    constexpr char suffix[] = "\n";
    return sendBytes(prefix, sizeof(prefix) - 1) &&
           sendBytes(cardJson, std::strlen(cardJson)) &&
           sendBytes(suffix, sizeof(suffix) - 1) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(500)) == ESP_OK;
}

bool displayCommunicationSendCardInfoFailed()
{
    const char* message = "CARD_INFO_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendCardImage(const uint8_t* data, size_t length)
{
    if (data == nullptr || length == 0) {
        return false;
    }
    char header[32];
    const int headerLength = std::snprintf(header, sizeof(header), "CARD_IMAGE %u\n",
                                           static_cast<unsigned>(length));
    return sendBytes(header, static_cast<size_t>(headerLength)) &&
           sendBytes(reinterpret_cast<const char*>(data), length) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(3000)) == ESP_OK;
}

bool displayCommunicationSendCardImageFailed()
{
    const char* message = "CARD_IMAGE_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendCamImage(const uint8_t* data, size_t length)
{
    if (data == nullptr || length == 0) {
        return false;
    }
    char header[32];
    const int headerLength = std::snprintf(header, sizeof(header), "CAM_IMAGE %u\n",
                                           static_cast<unsigned>(length));
    return sendBytes(header, static_cast<size_t>(headerLength)) &&
           sendBytes(reinterpret_cast<const char*>(data), length) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(5000)) == ESP_OK;
}

bool displayCommunicationSendCamPhoto(const uint8_t* data, size_t length)
{
    if (data == nullptr || length == 0) {
        return false;
    }
    char header[32];
    const int headerLength = std::snprintf(header, sizeof(header), "CAM_PHOTO %u\n",
                                           static_cast<unsigned>(length));
    return sendBytes(header, static_cast<size_t>(headerLength)) &&
           sendBytes(reinterpret_cast<const char*>(data), length) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(5000)) == ESP_OK;
}

bool displayCommunicationSendCaptureFailed()
{
    const char* message = "CAPTURE_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendCardRemoveResult(bool success)
{
    const char* message = success ? "CARD_REMOVE_DONE\n" : "CARD_REMOVE_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendImportResult(bool success)
{
    const char* message = success ? "IMPORT_DONE\n" : "IMPORT_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}

bool displayCommunicationSendExportResult(bool success)
{
    const char* message = success ? "EXPORT_DONE\n" : "EXPORT_FAILED\n";
    return sendBytes(message, std::strlen(message)) &&
           uart_wait_tx_done(DisplayUart, pdMS_TO_TICKS(100)) == ESP_OK;
}
