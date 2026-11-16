#pragma once

#include "Ili9341Display.hpp"

#include <string>

class MenuScreen {
public:
    void drawHome(Ili9341Display& display) const;
    void drawPage(Ili9341Display& display, const char* title) const;

    int hitTest(int x, int y) const;
    bool hitExit(int x, int y) const;

private:
    void drawRotatedText(Ili9341Display& display, const char* text,
                         int centerX, int centerY, uint16_t color) const;
    void drawGlyph(Ili9341Display& display, char character, int x, int y,
                   uint16_t color) const;
};
