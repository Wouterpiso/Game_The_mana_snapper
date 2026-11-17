#include "CamCommunication.hpp"

#include "driver/uart.h"

namespace {
constexpr uart_port_t CamUart = UART_NUM_2;
constexpr int CamTxPin = 25;
constexpr int CamRxPin = 26;
constexpr int BaudRate = 115200;
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
    ESP_ERROR_CHECK(uart_driver_install(CamUart, 2048, 0, 0, nullptr, 0));
}

bool camCommunicationHasData()
{
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(CamUart, &available));
    return available > 0;
}

void camCommunicationHandle()
{
    uint8_t data[128];
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(CamUart, &available));

    const size_t bytesToRead = available < sizeof(data) ? available : sizeof(data);
    if (bytesToRead > 0) {
        const int bytesRead = uart_read_bytes(CamUart, data, bytesToRead, 0);
        if (bytesRead < 0) {
            return;
        }
    }
}