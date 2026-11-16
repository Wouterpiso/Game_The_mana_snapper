#include "MenuScreen.hpp"

#include <cstring>

namespace {
constexpr int kButtonX = 105;
constexpr int kButtonWidth = 110;
constexpr int kButtonHeight = 48;
constexpr int kButtonYs[] = {18, 96, 174};
constexpr int kVisualButtonXs[] = {32, 136, 240};

const uint8_t* glyph(char character) {
    static const uint8_t patterns[][5] = {
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
        {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
        {0x46, 0x49, 0x49, 0x49, 0x31}, // S
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
        {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
        {0x63, 0x14, 0x08, 0x14, 0x63}, // X
        {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
        {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    };
    if (character >= 'A' && character <= 'Z') {
        return patterns[character - 'A'];
    }
    return nullptr;
}
}

void MenuScreen::drawGlyph(Ili9341Display& display, char character, int x,
                           int y, uint16_t color) const {
    const uint8_t* pattern = glyph(character);
    if (pattern == nullptr) {
        return;
    }
    for (int column = 0; column < 5; ++column) {
        for (int row = 0; row < 7; ++row) {
            if ((pattern[column] & (1 << row)) != 0) {
                display.drawPixel(x + column, y + row, color);
            }
        }
    }
}

void MenuScreen::drawRotatedText(Ili9341Display& display, const char* text,
                                 int centerX, int centerY,
                                 uint16_t color) const {
    constexpr int glyphWidth = 5;
    constexpr int glyphHeight = 7;
    constexpr int spacing = 1;
    const int length = static_cast<int>(std::strlen(text));
    const int sourceWidth = length * (glyphWidth + spacing) - spacing;
    const int left = centerX - glyphHeight / 2;
    const int top = centerY - sourceWidth / 2;

    for (int characterIndex = 0; characterIndex < length; ++characterIndex) {
        const uint8_t* pattern = glyph(text[characterIndex]);
        if (pattern == nullptr) {
            continue;
        }
        const int sourceLeft = characterIndex * (glyphWidth + spacing);
        for (int sourceX = 0; sourceX < glyphWidth; ++sourceX) {
            for (int sourceY = 0; sourceY < glyphHeight; ++sourceY) {
                if ((pattern[sourceX] & (1 << sourceY)) != 0) {
                    display.drawPixel(left + sourceY,
                                      top + sourceWidth - 1 - sourceLeft -
                                          sourceX,
                                      color);
                }
            }
        }
    }
}

void MenuScreen::drawHome(Ili9341Display& display) const {
    const uint16_t white = display.rgb565(255, 255, 255);
    display.fillScreen(display.rgb565(0, 0, 0));
    const char* labels[] = {"ADD CARD", "COLLECTION", "DECKS"};
    for (int index = 0; index < 3; ++index) {
        const int visualX = kVisualButtonXs[index];
        const int visualY =
            (Ili9341Display::HEIGHT - kButtonWidth) / 2;
        display.drawRectangle(visualX, visualY, kButtonHeight, kButtonWidth,
                              white, false);
        drawRotatedText(display, labels[index],
                        visualX + kButtonHeight / 2,
                        visualY + kButtonWidth / 2, white);
    }
    display.present();
}

void MenuScreen::drawPage(Ili9341Display& display, const char* title) const {
    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    display.fillScreen(black);
    display.drawRectangle(8, 8, 38, 30, white, true);
    drawRotatedText(display, "EXIT", 27, 23, black);
    drawRotatedText(display, title, Ili9341Display::WIDTH / 2,
                    Ili9341Display::HEIGHT / 2, white);
    display.present();
}

int MenuScreen::hitTest(int x, int y) const {
    constexpr int touchPaddingX = 10;
    constexpr int touchPaddingY = 10;
    for (int index = 0; index < 3; ++index) {
        if (x >= kButtonX - touchPaddingX &&
            x < kButtonX + kButtonWidth + touchPaddingX &&
            y >= kButtonYs[index] - touchPaddingY &&
            y < kButtonYs[index] + kButtonHeight + touchPaddingY) {
            return index;
        }
    }
    return -1;
}

bool MenuScreen::hitExit(int x, int y) const {
    return x >= 8 && x < 46 && y >= 8 && y < 38;
}
