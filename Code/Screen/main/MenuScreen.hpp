/**
 * @file MenuScreen.hpp
 * @brief Drawing and touch hit-testing for every screen of the UI.
 */
#pragma once

#include "St7789Display.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

/**
 * @brief Draws the user interface and maps touches to actions.
 *
 * The class owns the data of the last collection and card response, so a page
 * can be redrawn after a touch without asking the Master again.
 */
class MenuScreen {
public:
  /**
   * @brief Draws the main menu.
   * @param display Display to draw on.
   */
  void drawHome(St7789Display& display) const;

  /**
   * @brief Draws a page with a title and an EXIT button.
   * @param display Display to draw on.
   * @param title Page title.
   */
  void drawPage(St7789Display& display, const char* title) const;

  /**
   * @brief Draws the collection loading screen.
   * @param display Display to draw on.
   */
  void drawCollectionLoading(St7789Display& display) const;

  /**
   * @brief Draws the ADD CARD menu.
   * @param display Display to draw on.
   */
  void drawAddMenu(St7789Display& display) const;

  /**
   * @brief Draws the on-screen keyboard.
   * @param display Display to draw on.
   * @param text Text typed so far.
   * @param enterLabel Label of the enter key, used as a status line.
   */
  void drawKeyboard(St7789Display& display, const char* text,
                    const char* enterLabel = "ENTER") const;

  /**
   * @brief Draws the camera screen without a picture.
   * @param display Display to draw on.
   * @param message Message shown to the user.
   */
  void drawCameraWaiting(St7789Display& display, const char* message = "WAITING") const;

  /**
   * @brief Draws one live camera frame, scaled to the screen.
   * @param display Display to draw on.
   * @param jpeg JPEG bytes.
   * @param jpegLength Number of JPEG bytes.
   * @return False when the picture could not be decoded; the previous frame
   *         then stays on screen.
   */
  bool drawCameraView(St7789Display& display, const uint8_t* jpeg, size_t jpegLength) const;

  /**
   * @brief Draws the photo taken at SNAP with the demo disclaimer and BACK.
   * @param display Display to draw on.
   * @param jpeg JPEG bytes.
   * @param jpegLength Number of JPEG bytes.
   * @return False when the picture could not be decoded.
   */
  bool drawDemoPhoto(St7789Display& display, const uint8_t* jpeg, size_t jpegLength) const;

  /**
   * @brief Draws the overlay shown while the photo is being taken.
   * @param display Display to draw on.
   * @param label Status text.
   */
  void drawCaptureSending(St7789Display& display, const char* label = "TAKING PHOTO") const;

  /**
   * @brief Draws the OCR demo loading screen.
   * @param display Display to draw on.
   */
  void drawOcrLoading(St7789Display& display) const;

  /**
   * @brief Draws the OCR demo result.
   * @param display Display to draw on.
   * @param testImage Number of the test image that was used.
   * @param text Recognised card title.
   */
  void drawOcrResult(St7789Display& display, int testImage, const char* text) const;

  /**
   * @brief Draws the OCR demo failure screen.
   * @param display Display to draw on.
   */
  void drawOcrFailed(St7789Display& display) const;

  /**
   * @brief Stores and draws the printings of the found card.
   * @param display Display to draw on.
   * @param payload "<card name>|<set>|<number>|<set name>|..." groups.
   */
  void drawAddSets(St7789Display& display, const char* payload);

  /**
   * @brief Draws one page of the printing list.
   * @param display Display to draw on.
   * @param page Requested page, clamped to the number of pages.
   * @return Page that was actually shown.
   */
  int drawAddSetsPage(St7789Display& display, int page) const;

  /**
   * @brief Stores and draws the collection list.
   * @param display Display to draw on.
   * @param json Collection summary array.
   */
  void drawCollection(St7789Display& display, const char* json);

  /**
   * @brief Draws one page of the collection list.
   * @param display Display to draw on.
   * @param page Requested page, clamped to the number of pages.
   * @return Page that was actually shown.
   */
  int drawCollectionPage(St7789Display& display, int page) const;

  /**
   * @brief Draws the IMPORT / EXPORT menu.
   * @param display Display to draw on.
   */
  void drawTransferMenu(St7789Display& display) const;

  /**
   * @brief Stores the detail data of the selected card.
   * @param json Card JSON sent by the Master.
   */
  void setCardInfo(const char* json);

  /**
   * @brief Draws the card detail loading screen.
   * @param display Display to draw on.
   */
  void drawDetailLoading(St7789Display& display) const;

  /**
   * @brief Draws the card detail menu.
   * @param display Display to draw on.
   */
  void drawDetailMenu(St7789Display& display) const;

  /**
   * @brief Draws the image of the selected card.
   * @param display Display to draw on.
   * @param jpeg JPEG bytes, may be nullptr.
   * @param jpegLength Number of JPEG bytes.
   * @return False when the picture could not be decoded; a placeholder is
   *         drawn instead.
   */
  bool drawCardView(St7789Display& display, const uint8_t* jpeg, size_t jpegLength) const;

  /**
   * @brief Draws the remove confirmation with YES / NO / REMOVE ONE COPY.
   * @param display Display to draw on.
   */
  void drawRemoveConfirm(St7789Display& display) const;

  /**
   * @brief Draws the legalities of the selected card.
   * @param display Display to draw on.
   */
  void drawLegalities(St7789Display& display) const;

  /**
   * @brief Draws the remaining card fields.
   * @param display Display to draw on.
   */
  void drawOther(St7789Display& display) const;

  /**
   * @brief Returns the main-menu button under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return 0 = ADD CARD, 1 = COLLECTION, 2 = DECKS, -1 = none.
   */
  int hitTest(int x, int y) const;

  /**
   * @brief Returns the ADD CARD menu item under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return 0 = TYPE NAME, 1 = SCAN DEMO, 2 = OCR DEMO, -1 = none.
   */
  int hitAddMenu(int x, int y) const;

  /**
   * @brief Returns the keyboard key under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return 'A'..'Z', ' ', '\\b' (delete), '\\n' (ENTER), or 0 for no key.
   */
  char hitKeyboardKey(int x, int y) const;

  /**
   * @brief Checks the BACK button of the SNAP photo.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return True when the button was hit.
   */
  bool hitDemoBack(int x, int y) const;

  /**
   * @brief Returns the collection row under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @param page Page that is currently shown.
   * @return Card index, or -1 when no row was hit.
   */
  int hitCollectionRow(int x, int y, int page) const;

  /**
   * @brief Returns the printing row under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @param page Page that is currently shown.
   * @return Printing index, or -1 when no row was hit.
   */
  int hitAddSetRow(int x, int y, int page) const;

  /**
   * @brief Returns the card detail button under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return 0 = CARD, 1 = LEGALITIES, 2 = OTHER, 3 = REMOVE, -1 = none.
   */
  int hitDetailButton(int x, int y) const;

  /**
   * @brief Returns the remove confirmation button under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return 0 = YES (all copies), 1 = NO, 2 = REMOVE ONE COPY, -1 = none.
   */
  int hitRemoveConfirm(int x, int y) const;

  /**
   * @brief Checks the EXIT button.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return True when the button was hit.
   */
  bool hitExit(int x, int y) const;

  /**
   * @brief Checks the SNAP button.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return True when the button was hit.
   */
  bool hitCameraShutter(int x, int y) const;

  /**
   * @brief Checks the previous-page button.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return True when the button was hit.
   */
  bool hitCollectionPrevious(int x, int y) const;

  /**
   * @brief Checks the next-page button.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return True when the button was hit.
   */
  bool hitCollectionNext(int x, int y) const;

  /**
   * @brief Checks the IMPORT / EXPORT button.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return True when the button was hit.
   */
  bool hitTransfer(int x, int y) const;

  /**
   * @brief Returns the transfer menu item under a touch.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @return 0 = IMPORT, 1 = EXPORT, -1 = none.
   */
  int hitTransferMenu(int x, int y) const;

private:
  /**
   * @brief Decodes and scales a camera picture over the screen.
   * @param display Display to draw on.
   * @param jpeg JPEG bytes.
   * @param jpegLength Number of JPEG bytes.
   * @return False when the picture could not be decoded.
   */
  bool drawImage(St7789Display& display, const uint8_t* jpeg, size_t jpegLength) const;

  /**
   * @brief Draws a rectangle in reader coordinates.
   */
  void readerRect(St7789Display& display, int u, int v, int width, int height, uint16_t color,
                  bool fill) const;

  /**
   * @brief Draws a line in reader coordinates.
   */
  void readerLine(St7789Display& display, int u0, int v0, int u1, int v1, uint16_t color) const;

  /**
   * @brief Draws text in reader coordinates.
   */
  void readerText(St7789Display& display, const char* text, int u, int v, uint16_t color,
                  int scale) const;

  /**
   * @brief Draws the EXIT button.
   */
  void drawExitButton(St7789Display& display, const char* label = "EXIT") const;

  /**
   * @brief Draws the IMPORT / EXPORT button.
   */
  void drawTransferButton(St7789Display& display) const;

  /**
   * @brief Draws the title bar of a card detail page.
   */
  void drawDetailTitle(St7789Display& display, const char* title) const;

  /**
   * @brief Draws rotated text.
   */
  void drawRotatedText(St7789Display& display, const char* text, int centerX, int centerY,
                       uint16_t color) const;

  std::string collectionJson_; /**< Last collection summary array. */
  std::string cardInfoJson_;   /**< Last card detail object. */
  std::string addSetsPayload_; /**< Last printing list. */
  int collectionCount_ = 0;    /**< Number of cards in collectionJson_. */
  int addSetsCount_ = 0;       /**< Number of printings in addSetsPayload_. */
};
