#include "Ili9341Display.hpp"
#include "MenuScreen.hpp"
#include "StartScreenEntity.hpp"
#include "TouchReader.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
    constexpr int lcdMosi = 13;
    constexpr int lcdSclk = 14;
    constexpr int lcdCs = 15;
    constexpr int lcdDc = 2;
    constexpr int lcdRst = -1;
    constexpr int lcdBacklight = 21;

    Ili9341Display display(lcdMosi, lcdSclk, lcdCs, lcdDc, lcdRst, lcdBacklight);
    display.init();

    StartScreenEntity startScreen;
    display.fillScreen(display.rgb565(255, 255, 255));
    startScreen.draw(display);
    display.present();

    // TPM408-2.8 / XPT2046 touch wiring.
    TouchReader touch(SPI3_HOST, GPIO_NUM_25, GPIO_NUM_32, GPIO_NUM_39,
                      GPIO_NUM_33, GPIO_NUM_36);
    touch.start();

    while (!touch.touched()) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    touch.waitForRelease();

    MenuScreen menu;
    menu.drawHome(display);
    int page = -1;
    while (true) {
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
                menu.drawPage(display, labels[page]);
            }
        } else if (menu.hitExit(point.x, point.y)) {
            page = -1;
            menu.drawHome(display);
        }
    }
}
