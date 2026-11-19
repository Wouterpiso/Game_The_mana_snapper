#pragma once

#include "Ili9341Display.hpp"

#include <string>

class MenuScreen {
public:
    void drawHome(Ili9341Display& display) const;
    void drawPage(Ili9341Display& display, const char* title) const;
    void drawCollectionLoading(Ili9341Display& display) const;
    void drawAddCardLoading(Ili9341Display& display) const;
    void drawCollection(Ili9341Display& display, const char* json);
    // Draws one page of the collection and returns the page that was shown
    // (the requested page is clamped to the number of pages).
    int drawCollectionPage(Ili9341Display& display, int page) const;

    int hitTest(int x, int y) const;
    bool hitExit(int x, int y) const;
    bool hitCollectionPrevious(int x, int y) const;
    bool hitCollectionNext(int x, int y) const;

private:
    void readerRect(Ili9341Display& display, int u, int v, int width, int height,
                    uint16_t color, bool fill) const;
    void readerLine(Ili9341Display& display, int u0, int v0, int u1, int v1,
                    uint16_t color) const;
    void readerText(Ili9341Display& display, const char* text, int u, int v,
                    uint16_t color, int scale) const;
    void drawExitButton(Ili9341Display& display) const;
    void drawRotatedText(Ili9341Display& display, const char* text,
                         int centerX, int centerY, uint16_t color) const;
    void drawGlyph(Ili9341Display& display, char character, int x, int y,
                   uint16_t color) const;
    void drawText(Ili9341Display& display, const char* text, int x, int y,
                  uint16_t color, int scale = 1) const;

    std::string collectionJson_;
};
