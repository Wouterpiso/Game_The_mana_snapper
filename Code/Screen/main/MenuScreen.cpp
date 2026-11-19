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
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
        {0x62, 0x51, 0x49, 0x49, 0x46}, // 2
        {0x22, 0x41, 0x49, 0x49, 0x36}, // 3
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    };
    if (character >= '0' && character <= '9') {
        return digits[character - '0'];
    }
    static const uint8_t slash[] = {0x20, 0x10, 0x08, 0x04, 0x02};
    if (character == '/') {
        return slash;
    }
    return nullptr;
}

// The panel is used in portrait: the reader sees the 320x240 back buffer
// turned a quarter. All page content below is laid out in "reader" coordinates
// (u = right, 0..239; v = down, 0..319) and mapped onto the buffer here, the
// same way drawRotatedText() does it.
constexpr int kReaderWidth = Ili9341Display::HEIGHT;   // 240
inline int bufferX(int /*u*/, int v) { return v; }
inline int bufferY(int u, int /*v*/) { return kReaderWidth - 1 - u; }

// Collection page layout (reader coordinates).
constexpr int kCollectionFirstRowV = 58;
constexpr int kCollectionRowHeight = 24;
constexpr int kCollectionMaxRows = 8;
constexpr int kNavV = 284;
constexpr int kNavHeight = 26;
constexpr int kNavWidth = 48;
constexpr int kNavPreviousU = 24;
constexpr int kNavPageU = 96;
constexpr int kNavNextU = 168;
constexpr int kNavTouchPadding = 8;

// Width in pixels that drawText() needs for `text` (same advance rules).
int textWidth(const char* text, int scale) {
    int width = 0;
    for (const char* character = text; *character != '\0'; ++character) {
        width += (*character == ' ') ? 4 * scale : 6 * scale;
    }
    return width;
}

// Touch coordinates -> reader coordinates. The touch driver scales its two raw
// axes to 0..319 (x) and 0..239 (y), but x runs along the buffer height and y
// along the buffer width, so each one is stretched back to the buffer size.
void touchToReader(int touchX, int touchY, int& u, int& v) {
    const int bufferXPos = touchY * Ili9341Display::WIDTH / Ili9341Display::HEIGHT;
    const int bufferYPos = touchX * Ili9341Display::HEIGHT / Ili9341Display::WIDTH;
    u = kReaderWidth - 1 - bufferYPos;
    v = bufferXPos;
}

bool insideReaderRect(int touchX, int touchY, int u, int v, int width,
                      int height, int padding) {
    int touchU = 0;
    int touchV = 0;
    touchToReader(touchX, touchY, touchU, touchV);
    return touchU >= u - padding && touchU <= u + width + padding &&
           touchV >= v - padding && touchV <= v + height + padding;
}

uint16_t colorPipValue(Ili9341Display& display, char color) {
    switch (color) {
    case 'W':
        return display.rgb565(255, 255, 255);
    case 'U':
        return display.rgb565(30, 100, 255);
    case 'B':
        return display.rgb565(0, 0, 0);
    case 'R':
        return display.rgb565(230, 30, 30);
    case 'G':
        return display.rgb565(30, 180, 60);
    default:
        return display.rgb565(128, 128, 128);
    }
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

void MenuScreen::drawAddCardLoading(Ili9341Display& display) const {
    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    display.fillScreen(black);
    display.drawRectangle(8, 8, 38, 30, white, true);
    drawRotatedText(display, "EXIT", 27, 23, black);
    drawRotatedText(display, "ADD CARD", 32, 160, white);
    drawRotatedText(display, "LOADING", 160, 120, white);
    display.present();
}

void MenuScreen::drawCollection(Ili9341Display& display,
                                const char* json) {
    collectionJson_ = json == nullptr ? "" : json;
    drawCollectionPage(display, 0);
}

void MenuScreen::readerRect(Ili9341Display& display, int u, int v, int width,
                            int height, uint16_t color, bool fill) const {
    // A reader rectangle (width x height) is a (height x width) buffer rectangle.
    display.drawRectangle(bufferX(u, v), bufferY(u + width, v), height, width,
                          color, fill);
}

void MenuScreen::readerLine(Ili9341Display& display, int u0, int v0, int u1,
                            int v1, uint16_t color) const {
    display.drawLine(bufferX(u0, v0), bufferY(u0, v0), bufferX(u1, v1),
                     bufferY(u1, v1), color);
}

void MenuScreen::readerText(Ili9341Display& display, const char* text, int u,
                            int v, uint16_t color, int scale) const {
    int currentU = u;
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
                    for (int offsetU = 0; offsetU < scale; ++offsetU) {
                        for (int offsetV = 0; offsetV < scale; ++offsetV) {
                            const int pixelU = currentU + column * scale + offsetU;
                            const int pixelV = v + row * scale + offsetV;
                            display.drawPixel(bufferX(pixelU, pixelV),
                                              bufferY(pixelU, pixelV), color);
                        }
                    }
                }
            }
        }
        currentU += (*character == ' ') ? 4 * scale : 6 * scale;
    }
}

void MenuScreen::drawExitButton(Ili9341Display& display) const {
    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    display.drawRectangle(8, 8, 38, 30, white, true);
    drawRotatedText(display, "EXIT", 27, 23, black);
}

int MenuScreen::drawCollectionPage(Ili9341Display& display, int page) const {
    cJSON* cards = cJSON_Parse(collectionJson_.c_str());
    if (cards == nullptr || !cJSON_IsArray(cards)) {
        cJSON_Delete(cards);
        const uint16_t black = display.rgb565(0, 0, 0);
        const uint16_t white = display.rgb565(255, 255, 255);
        display.fillScreen(black);
        drawExitButton(display);
        drawRotatedText(display, "COLLECTION", 32, 160, white);
        drawRotatedText(display, "DATA ERROR", 160, 120, white);
        display.present();
        return 0;
    }

    const uint16_t black = display.rgb565(0, 0, 0);
    const uint16_t white = display.rgb565(255, 255, 255);
    const uint16_t separator = display.rgb565(60, 60, 60);
    const uint16_t outline = display.rgb565(140, 140, 140);
    display.fillScreen(black);
    drawExitButton(display);
    drawRotatedText(display, "COLLECTION", 32, 160, white);

    constexpr int margin = 6;
    constexpr int pipWidth = 8;
    constexpr int pipHeight = 14;
    constexpr int pipSpacing = 3;
    constexpr int maxPips = 5;
    const int totalCards = cJSON_GetArraySize(cards);
    const int totalPages =
        std::max(1, (totalCards + kCollectionMaxRows - 1) / kCollectionMaxRows);
    const int safePage = std::max(0, std::min(page, totalPages - 1));
    const int firstCard = safePage * kCollectionMaxRows;
    const int cardCount = std::min(totalCards - firstCard, kCollectionMaxRows);
    for (int index = 0; index < cardCount; ++index) {
        cJSON* card = cJSON_GetArrayItem(cards, firstCard + index);
        cJSON* name = cJSON_GetObjectItemCaseSensitive(card, "name");
        cJSON* colors = cJSON_GetObjectItemCaseSensitive(card, "colors");
        if (!cJSON_IsString(name) || !cJSON_IsArray(colors)) {
            continue;
        }

        const int rowV = kCollectionFirstRowV + index * kCollectionRowHeight;

        // Colour pips on the right, one per colour (white/blue/black/red/green).
        const int pipCount = std::min(cJSON_GetArraySize(colors), maxPips);
        const int pipsWidth =
            pipCount * pipWidth + std::max(0, pipCount - 1) * pipSpacing;
        const int pipsStartU = kReaderWidth - margin - pipsWidth;
        int drawnPips = 0;
        for (int colorIndex = 0; colorIndex < pipCount; ++colorIndex) {
            cJSON* color = cJSON_GetArrayItem(colors, colorIndex);
            if (!cJSON_IsString(color) || color->valuestring[0] == '\0') {
                continue;
            }
            const int pipU = pipsStartU + drawnPips * (pipWidth + pipSpacing);
            readerRect(display, pipU, rowV, pipWidth, pipHeight,
                       colorPipValue(display, color->valuestring[0]), true);
            // Outline so a black pip stays visible on the black background.
            readerRect(display, pipU, rowV, pipWidth, pipHeight, outline, false);
            ++drawnPips;
        }

        // Name on the left: big text when it fits, small text otherwise.
        const int nameWidthLimit =
            (pipCount > 0 ? pipsStartU - margin : kReaderWidth - margin) - margin;
        char displayName[48] = {};
        std::strncpy(displayName, name->valuestring, sizeof(displayName) - 1);
        int scale = textWidth(displayName, 2) <= nameWidthLimit ? 2 : 1;
        while (textWidth(displayName, scale) > nameWidthLimit &&
               std::strlen(displayName) > 0) {
            displayName[std::strlen(displayName) - 1] = '\0';
        }
        const int textV = rowV + (scale == 2 ? 0 : 4);
        readerText(display, displayName, margin, textV, white, scale);

        readerLine(display, margin, rowV + 19, kReaderWidth - margin, rowV + 19,
                   separator);
    }

    if (cardCount == 0) {
        const int width = textWidth("NO CARDS", 2);
        readerText(display, "NO CARDS", (kReaderWidth - width) / 2, 150, white, 2);
    }

    // Previous / page indicator / next.
    const uint16_t buttonColor = display.rgb565(80, 80, 80);
    readerRect(display, kNavPreviousU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
    readerLine(display, kNavPreviousU + 28, kNavV + 7, kNavPreviousU + 20,
               kNavV + 13, white);
    readerLine(display, kNavPreviousU + 20, kNavV + 13, kNavPreviousU + 28,
               kNavV + 19, white);

    readerRect(display, kNavPageU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
    char pageText[24] = {};
    std::snprintf(pageText, sizeof(pageText), "%d/%d", safePage + 1, totalPages);
    const int pageScale = textWidth(pageText, 2) <= kNavWidth - 6 ? 2 : 1;
    const int pageTextWidth = textWidth(pageText, pageScale);
    readerText(display, pageText, kNavPageU + (kNavWidth - pageTextWidth) / 2,
               kNavV + (kNavHeight - 7 * pageScale) / 2 + 1, white, pageScale);

    readerRect(display, kNavNextU, kNavV, kNavWidth, kNavHeight, buttonColor, false);
    readerLine(display, kNavNextU + 20, kNavV + 7, kNavNextU + 28, kNavV + 13,
               white);
    readerLine(display, kNavNextU + 28, kNavV + 13, kNavNextU + 20, kNavV + 19,
               white);

    display.present();
    cJSON_Delete(cards);
    return safePage;
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
    return insideReaderRect(x, y, kNavPreviousU, kNavV, kNavWidth, kNavHeight,
                            kNavTouchPadding);
}

bool MenuScreen::hitCollectionNext(int x, int y) const {
    return insideReaderRect(x, y, kNavNextU, kNavV, kNavWidth, kNavHeight,
                            kNavTouchPadding);
}
