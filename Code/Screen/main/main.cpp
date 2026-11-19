#include "Ili9341Display.hpp"
#include "MenuScreen.hpp"
#include "StartScreenEntity.hpp"
#include "TouchReader.hpp"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <stdio.h>

namespace {
constexpr char Tag[] = "ScreenCommunication";
constexpr uart_port_t MasterUart = UART_NUM_2;
constexpr int MasterTxPin = 22;
constexpr int MasterRxPin = 27;
constexpr int BaudRate = 115200;
constexpr size_t MasterReceiveBufferSize = 4096;

char masterReceiveBuffer[MasterReceiveBufferSize];
size_t masterReceiveLength = 0;

void masterCommunicationInit()
{
    const uart_config_t config = {
        .baud_rate = BaudRate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(MasterUart, &config));
    ESP_ERROR_CHECK(
        uart_set_pin(MasterUart, MasterTxPin, MasterRxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(MasterUart, 2048, 2048, 0, nullptr, 0));
}

void sendMasterCommand(const char* command)
{
    const size_t commandLength = std::strlen(command);
    ESP_LOGE(Tag, "TX command: %s", command);
    const int bytesWritten =
        uart_write_bytes(MasterUart, command, commandLength);
    if (bytesWritten != static_cast<int>(commandLength)) {
        ESP_LOGE(Tag, "Failed to send command");
        return;
    }
    ESP_ERROR_CHECK(uart_wait_tx_done(MasterUart, pdMS_TO_TICKS(100)));
}

enum class MasterEvent {
    None,
    AddCardDone,
    AddCardFailed,
};

MasterEvent receiveMasterData(Ili9341Display& display, MenuScreen& menu)
{
    MasterEvent event = MasterEvent::None;
    size_t available = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(MasterUart, &available));
    if (available == 0) {
        return event;
    }

    uint8_t data[512];
    const size_t bytesToRead =
        available < sizeof(data) - 1 ? available : sizeof(data) - 1;
        const int bytesRead = uart_read_bytes(MasterUart, data, bytesToRead, 0);
    if (bytesRead <= 0) {
        return event;
    }
    ESP_LOGE(Tag, "RX %d bytes", bytesRead);                       // NEW
    for (int index = 0; index < bytesRead; ++index) {
        const char character = static_cast<char>(data[index]);
        if (character == '\n') {
            masterReceiveBuffer[masterReceiveLength] = '\0';
            ESP_LOGE(Tag, "RX line (%u chars): %s",                // NEW
                     (unsigned)masterReceiveLength, masterReceiveBuffer);
            constexpr char prefix[] = "COLLECTION_LIST ";
            const char* response = std::strstr(masterReceiveBuffer, prefix);
            if (std::strstr(masterReceiveBuffer, "ADD_CARD_DONE") != nullptr) {
                event = MasterEvent::AddCardDone;
            } else if (std::strstr(masterReceiveBuffer, "ADD_CARD_FAILED") != nullptr) {
                event = MasterEvent::AddCardFailed;
            } else if (response != nullptr) {
                ESP_LOGE(Tag, "COLLECTION_LIST matched, drawing"); // NEW
                menu.drawCollection(display,
                                    response + sizeof(prefix) - 1);
            } else {
                ESP_LOGE(Tag, "line has no COLLECTION_LIST prefix"); // NEW
            }
            masterReceiveLength = 0;
            continue;
        }
        if (masterReceiveLength < MasterReceiveBufferSize - 1) {
            masterReceiveBuffer[masterReceiveLength++] = character;
        } else {
            masterReceiveLength = 0;
            ESP_LOGE(Tag, "Master response exceeded receive buffer");
        }
    }
    return event;
}
}

extern "C" void app_main(void) {
    constexpr int lcdMosi = 13;
    constexpr int lcdSclk = 14;
    constexpr int lcdCs = 15;
    constexpr int lcdDc = 2;
    constexpr int lcdRst = -1;
    constexpr int lcdBacklight = 21;

    masterCommunicationInit();

    Ili9341Display display(lcdMosi, lcdSclk, lcdCs, lcdDc, lcdRst, lcdBacklight);
    display.init();

    StartScreenEntity startScreen;
    display.fillScreen(display.rgb565(255, 255, 255));
    startScreen.draw(display);
    display.present();
    sendMasterCommand("IDLE\n");

    MenuScreen menu;
    // TPM408-2.8 / XPT2046 touch wiring.
    TouchReader touch(SPI3_HOST, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_39,
                      GPIO_NUM_33, GPIO_NUM_36);
    touch.start();

    while (!touch.touched()) {
        receiveMasterData(display, menu);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    touch.waitForRelease();

    menu.drawHome(display);
    sendMasterCommand("IDLE\n");
    int page = -1;
    int collectionPage = 0;
    while (true) {
        const MasterEvent masterEvent = receiveMasterData(display, menu);
        if (page == 0 && masterEvent != MasterEvent::None) {
            // The master finished the ADD CARD job: back to the main menu.
            if (masterEvent == MasterEvent::AddCardFailed) {
                menu.drawPage(display, "FAILED");
                vTaskDelay(pdMS_TO_TICKS(1500));
            }
            page = -1;
            menu.drawHome(display);
            sendMasterCommand("IDLE\n");
            continue;
        }
        if (!touch.touched()) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        const TouchReader::Point point = touch.readPoint();
        touch.waitForRelease();
        if (page < 0) {
            page = menu.hitTest(point.x, point.y);
            if (page >= 0) {
                static constexpr const char* labels[] = {
                    "ADD CARD", "COLLECTION", "DECKS"};
                static constexpr const char* commands[] = {
                    "ADD_CARD\n", "COLLECTION\n", "DECKS\n"};
                menu.drawPage(display, labels[page]);
                if (page == 0) {
                    menu.drawAddCardLoading(display);
                } else if (page == 1) {
                    menu.drawCollectionLoading(display);
                }
                sendMasterCommand(commands[page]);
            }
        } else if (page == 1 && menu.hitCollectionPrevious(point.x, point.y)) {
            if (collectionPage > 0) {
                collectionPage = menu.drawCollectionPage(display, collectionPage - 1);
            }
        } else if (page == 1 && menu.hitCollectionNext(point.x, point.y)) {
            collectionPage = menu.drawCollectionPage(display, collectionPage + 1);
        } else if (menu.hitExit(point.x, point.y)) {
            page = -1;
            collectionPage = 0;
            menu.drawHome(display);
            sendMasterCommand("IDLE\n");
        }
    }
}
