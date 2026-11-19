#pragma once

#include "Ili9341Display.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

class MenuScreen {
public:
    void drawHome(Ili9341Display& display) const;
    void drawPage(Ili9341Display& display, const char* title) const;
    void drawCollectionLoading(Ili9341Display& display) const;
    // ADD CARD menu: TYPE NAME / SCAN DEMO / OCR DEMO.
    void drawAddMenu(Ili9341Display& display) const;
    // On-screen keyboard for typing a card name. `enterLabel` normally reads
    // ENTER; it turns into a status line while the name is being looked up.
    void drawKeyboard(Ili9341Display& display, const char* text,
                      const char* enterLabel = "ENTER") const;
    // SCAN DEMO screens.
    void drawCameraWaiting(Ili9341Display& display,
                           const char* message = "WAITING") const;
    // One live camera frame, scaled to the screen. Returns false when the
    // picture could not be decoded (the previous frame stays on screen).
    bool drawCameraView(Ili9341Display& display, const uint8_t* jpeg, size_t jpegLength) const;
    // The photo taken at SNAP: the picture plus the demo disclaimer and BACK.
    // Returns false when the picture could not be decoded.
    bool drawDemoPhoto(Ili9341Display& display, const uint8_t* jpeg,
                       size_t jpegLength) const;
    // Overlays drawn on top of the photo while it is being taken.
    void drawCaptureSending(Ili9341Display& display,
                            const char* label = "TAKING PHOTO") const;
    // OCR DEMO screens.
    void drawOcrLoading(Ili9341Display& display) const;
    void drawOcrResult(Ili9341Display& display, int testImage, const char* text) const;
    void drawOcrFailed(Ili9341Display& display) const;

    // Every set the found card was printed in: "<card name>|<set>|<number>|
    // <set name>|..." (one group of three per printing), 8 rows per page.
    void drawAddSets(Ili9341Display& display, const char* payload);
    // Draws one page of the set list and returns the page that was shown
    // (the requested page is clamped to the number of pages).
    int drawAddSetsPage(Ili9341Display& display, int page) const;

    void drawCollection(Ili9341Display& display, const char* json);
    // Draws one page of the collection and returns the page that was shown
    // (the requested page is clamped to the number of pages).
    int drawCollectionPage(Ili9341Display& display, int page) const;
    // IMPORT / EXPORT menu opened from the collection page.
    void drawTransferMenu(Ili9341Display& display) const;

    // Details of one card. The Master sends the card as JSON; the views below
    // all draw from it.
    void setCardInfo(const char* json);
    void drawDetailLoading(Ili9341Display& display) const;
    void drawDetailMenu(Ili9341Display& display) const;
    // Returns false when the picture could not be decoded (a placeholder is drawn).
    bool drawCardView(Ili9341Display& display, const uint8_t* jpeg, size_t jpegLength) const;
    // "REMOVE CARD?" with the card's name and YES / NO / REMOVE ONE COPY.
    void drawRemoveConfirm(Ili9341Display& display) const;
    void drawLegalities(Ili9341Display& display) const;
    void drawOther(Ili9341Display& display) const;

    int hitTest(int x, int y) const;
    // 0 = TYPE NAME, 1 = SCAN DEMO, 2 = OCR DEMO, -1 = none.
    int hitAddMenu(int x, int y) const;
    // The key under the touch: 'A'..'Z', ' ' (space), '\b' (delete),
    // '\n' (ENTER) or 0 when no key was hit.
    char hitKeyboardKey(int x, int y) const;
    // The BACK button under the SCAN DEMO disclaimer.
    bool hitDemoBack(int x, int y) const;
    // Number of the card whose row was touched on `page`, or -1.
    int hitCollectionRow(int x, int y, int page) const;
    // Number of the printing whose row was touched on `page`, or -1.
    int hitAddSetRow(int x, int y, int page) const;
    // 0 = CARD, 1 = LEGALITIES, 2 = OTHER, 3 = REMOVE, -1 = none.
    int hitDetailButton(int x, int y) const;
    // 0 = YES (all copies), 1 = NO, 2 = REMOVE ONE COPY, -1 = none.
    int hitRemoveConfirm(int x, int y) const;
    bool hitExit(int x, int y) const;
    // The SNAP button under the camera preview.
    bool hitCameraShutter(int x, int y) const;
    bool hitCollectionPrevious(int x, int y) const;
    bool hitCollectionNext(int x, int y) const;
    // The IMPORT/EXPORT button in the top-left corner of the collection page.
    bool hitTransfer(int x, int y) const;
    // 0 = IMPORT, 1 = EXPORT, -1 = none.
    int hitTransferMenu(int x, int y) const;

private:
    // Decodes a camera picture and scales it over the screen. Returns false
    // when it could not be decoded (nothing is drawn in that case).
    bool drawImage(Ili9341Display& display, const uint8_t* jpeg, size_t jpegLength) const;
    void readerRect(Ili9341Display& display, int u, int v, int width, int height,
                    uint16_t color, bool fill) const;
    void readerLine(Ili9341Display& display, int u0, int v0, int u1, int v1,
                    uint16_t color) const;
    void readerText(Ili9341Display& display, const char* text, int u, int v,
                    uint16_t color, int scale) const;
    void drawExitButton(Ili9341Display& display, const char* label = "EXIT") const;
    void drawTransferButton(Ili9341Display& display) const;
    void drawDetailTitle(Ili9341Display& display, const char* title) const;
    void drawRotatedText(Ili9341Display& display, const char* text,
                         int centerX, int centerY, uint16_t color) const;
    void drawGlyph(Ili9341Display& display, char character, int x, int y,
                   uint16_t color) const;
    void drawText(Ili9341Display& display, const char* text, int x, int y,
                  uint16_t color, int scale = 1) const;

    std::string collectionJson_;
    std::string cardInfoJson_;
    std::string addSetsPayload_;
    int collectionCount_ = 0;
    int addSetsCount_ = 0;
};
