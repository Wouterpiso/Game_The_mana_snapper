#include "MenuScreen.hpp"

#include "cJSON.h"

#include <algorithm>
#include <cctype>
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
    static const uint8_t digits[][5] = {
        {0x3E, 0x45, 0x49, 0x51, 0x3E}, // 0
        {0x00, 0x21, 0x7F, 0x01, 0x00}, // 1
        {0x23, 0x45, 0x49, 0x49, 0x31}, // 2
        {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
        {0x0C, 0x14, 0x24, 0x7F, 0x04}, // 4
        {0x72, 0x51, 0x51, 0x51, 0x4E}, // 5
        {0x1E, 0x29, 0x49, 0x49, 0x06}, // 6
        {0x40, 0x47, 0x48, 0x50, 0x60}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x30, 0x49, 0x49, 0x4A, 0x3C}, // 9
    };
    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }
    static const uint8_t slash[] = {0x04, 0x08, 0x10, 0x20, 0x40};
    if (character == '/') {
        return slash;
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

void MenuScreen::drawText(Ili9341Display& display, const char* text, int x,
                          int y, uint16_t color, int scale) const {
    int currentX = x;
    for (const char* character = text; *character != '\0'; ++character) {
        const char upper =
            static_cast<char>(std::toupper(static_cast<unsigned char>(*character)));
        const uint8_t* pattern = glyph(upper);
        if (pattern != nullptr) {
            for (int column = 0; column < 5; ++column) {
                for (int row = 0; row < 7; ++row) {
                    if ((pattern[column] & (1 << row)) == 0) {
                        continue;
                    }
                    for (int offsetX = 0; offsetX < scale; ++offsetX) {
                        for (int offsetY = 0; offsetY < scale; ++offsetY) {
                            display.drawPixel(currentX + column * scale + offsetX,
                                              y + row * scale + offsetY, color);
                        }
                    }
                }
            }
        }
        currentX += (*character == ' ') ? 4 * scale : 6 * scale;
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

void MenuScreen::drawCollectionLoading(Ili9341Display& display) const {
    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    display.fillScreen(black);
    display.drawRectangle(8, 8, 38, 30, white, true);
    drawRotatedText(display, "EXIT", 27, 23, black);
    drawRotatedText(display, "COLLECTION", 32, 160, white);
    drawRotatedText(display, "LOADING", 160, 120, white);
    display.present();
}

void MenuScreen::drawCollection(Ili9341Display& display,
                                const char* json) {
    collectionJson_ = json == nullptr ? "" : json;
    drawCollectionPage(display, 0);
}

void MenuScreen::drawCollectionPage(Ili9341Display& display, int page) const {
    cJSON* cards = cJSON_Parse(collectionJson_.c_str());
    if (cards == nullptr || !cJSON_IsArray(cards)) {
        cJSON_Delete(cards);
        const uint16_t black = display.rgb565(0, 0, 0);
        const uint16_t white = display.rgb565(255, 255, 255);
        display.fillScreen(black);
        display.drawRectangle(8, 8, 38, 30, white, true);
        drawRotatedText(display, "EXIT", 27, 23, black);
        drawRotatedText(display, "COLLECTION", 32, 160, white);
        drawRotatedText(display, "DATA ERROR", 160, 120, white);
        display.present();
        return;
    }

    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    display.fillScreen(black);
    drawRotatedText(display, "COLLECTION", 32, 160, white);

    constexpr int firstRowY = 35;
    constexpr int rowHeight = 24;
    constexpr int maxRows = 8;
    const int totalCards = cJSON_GetArraySize(cards);
    const int totalPages = std::max(1, (totalCards + maxRows - 1) / maxRows);
    const int safePage = std::max(0, std::min(page, totalPages - 1));
    const int firstCard = safePage * maxRows;
    const int cardCount =
        std::min(totalCards - firstCard, maxRows);
    for (int index = 0; index < cardCount; ++index) {
        cJSON* card = cJSON_GetArrayItem(cards, firstCard + index);
        cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
        cJSON* colors = cJSON_GetObjectItemCaseSensitive(card, "colors");
        if (!cJSON_IsString(name) || !cJSON_IsArray(colors)) {
            continue;
        }

        char displayName[30] = {};
        std::strncpy(displayName, name->valuestring,
                     sizeof(displayName) - 1);
        displayName[23] = '\0';
        const int rowY = firstRowY + index * rowHeight;
        drawText(display, displayName, 8, rowY, white, 2);

        const int colorCount = std::min(cJSON_GetArraySize(colors), 5);
        for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
            cJSON* color = cJSON_GetArrayItem(colors, colorIndex);
            if (!cJSON_IsString(color) || color->valuestring[0] == '\0') {
                continue;
            }

            uint16_t colorValue = display.rgb565(128, 128, 128);
            switch (color->valuestring[0]) {
            case 'W':
                colorValue = display.rgb565(255, 255, 255);
                break;
            case 'U':
                colorValue = display.rgb565(40, 100, 255);
                break;
            case 'B':
                colorValue = display.rgb565(100, 100, 100);
                break;
            case 'R':
                colorValue = display.rgb565(255, 50, 50);
                break;
            case 'G':
                colorValue = display.rgb565(50, 220, 80);
                break;
            default:
                break;
            }
            display.drawRectangle(270 + colorIndex * 10, rowY + 2, 6, 12,
                                  colorValue, true);
        }
        display.drawLine(8, rowY + 19, 312, rowY + 19,
                         display.rgb565(60, 60, 60));
    }

    if (cardCount == 0) {
        drawText(display, "NO CARDS", 112, 110, white, 2);
    }
    const uint16_t buttonColor = display.rgb565(80, 80, 80);
    display.drawRectangle(55, 207, 42, 25, buttonColor, false);
    display.drawLine(84, 213, 70, 219, white);
    display.drawLine(70, 219, 84, 225, white);
    display.drawRectangle(136, 207, 48, 25, buttonColor, false);
    char pageText[24] = {};
    std::snprintf(pageText, sizeof(pageText), "%d/%d", safePage + 1,
                  totalPages);
    drawText(display, pageText, 145, 215, white, 1);
    display.drawRectangle(223, 207, 42, 25, buttonColor, false);
    display.drawLine(236, 213, 250, 219, white);
    display.drawLine(250, 219, 236, 225, white);
    display.present();
    cJSON_Delete(cards);
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

bool MenuScreen::hitCollectionPrevious(int x, int y) const {
    return x >= 55 && x < 97 && y >= 207 && y < 232;
}

bool MenuScreen::hitCollectionNext(int x, int y) const {
    return x >= 223 && x < 265 && y >= 207 && y < 232;
}
