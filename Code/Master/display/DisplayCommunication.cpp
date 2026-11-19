#include "DisplayCommunication.hpp"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <stdio.h>

namespace {
constexpr uart_port_t DisplayUart = UART_NUM_1;
constexpr int DisplayTxPin = 17;
constexpr int DisplayRxPin = 16;
constexpr int BaudRate = 115200;

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
    if (length == 8 && std::strncmp(line, "ADD_CARD", 8) == 0) {
        return DisplayRequest::Cam;
    }
    if (length == 10 && std::strncmp(line, "COLLECTION", 10) == 0) {
        return DisplayRequest::Collection;
    }
    if (length == 5 && std::strncmp(line, "DECKS", 5) == 0) {
        return DisplayRequest::Display;
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
    return DisplayRequest::Idle;
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
