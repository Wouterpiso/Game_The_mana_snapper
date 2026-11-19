/**
 * @file St7789Display.hpp
 * @brief SPI LCD wrapper with a full-frame backbuffer.
 *
 * The wrapper drives the 2.8 inch TPM408 panel, which uses an ST7789
 * controller. All drawing goes into a DMA-capable backbuffer; present() sends
 * one complete frame to the panel to avoid flicker.
 */
#pragma once

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h" // for heap_caps_malloc
#include "esp_timer.h"     // for esp_timer_get_time for fine grain timing

/**
 * @brief LCD wrapper that draws into a backbuffer and presents whole frames.
 */
class St7789Display {
public:
  /**
   * @brief Creates a display wrapper; init() performs the actual setup.
   * @param mosi SPI MOSI pin.
   * @param sclk SPI clock pin.
   * @param cs LCD chip-select pin.
   * @param dc LCD data/command pin.
   * @param rst LCD reset pin, or -1 when the panel has no reset line.
   * @param bl Backlight pin, or -1 when the backlight is not controlled.
   */
  St7789Display(int mosi, int sclk, int cs, int dc, int rst, int bl);

  /**
   * @brief Releases the backbuffer and turns the backlight off.
   */
  ~St7789Display();

  static constexpr int WIDTH = 320;  /**< Screen width in pixels (landscape). */
  static constexpr int HEIGHT = 240; /**< Screen height in pixels (landscape). */

  /**
   * @brief Sets up SPI, the panel and the backbuffer.
   */
  void init();

  /**
   * @brief Fills the backbuffer with one color.
   * @param color RGB565 color.
   */
  void fillScreen(uint16_t color);

  /**
   * @brief Turns the backlight on.
   */
  void backlightOn();

  /**
   * @brief Turns the backlight off.
   */
  void backlightOff();

  /**
   * @brief Converts 8-bit RGB components to RGB565.
   * @param r Red component.
   * @param g Green component.
   * @param b Blue component.
   * @return RGB565 color.
   */
  uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

  /**
   * @brief Draws one pixel into the backbuffer.
   * @param x Screen x coordinate.
   * @param y Screen y coordinate.
   * @param color RGB565 color.
   */
  void drawPixel(int x, int y, uint16_t color);

  /**
   * @brief Draws a line with Bresenham's algorithm.
   */
  void drawLine(int x0, int y0, int x1, int y1, uint16_t color);

  /**
   * @brief Draws a rectangle.
   * @param fill True to fill the rectangle, false for an outline.
   */
  void drawRectangle(int x, int y, int w, int h, uint16_t color, bool fill = false);

  /**
   * @brief Draws a triangle.
   * @param fill True to fill the triangle, false for an outline.
   */
  void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color,
                    bool fill = false);

  /**
   * @brief Draws a circle.
   * @param fill True to fill the circle, false for an outline.
   */
  void drawCircle(int x, int y, int radius, uint16_t color, bool fill = false);

  /**
   * @brief Draws an RGB565 bitmap from a little-endian byte stream.
   * @param x Left position.
   * @param y Top position.
   * @param w Bitmap width.
   * @param h Bitmap height.
   * @param src Bitmap bytes.
   */
  void drawBitmapRGB565(int x, int y, int w, int h, const uint8_t* src);

  /**
   * @brief Draws an RGB565 bitmap from a 16-bit buffer.
   * @param x Left position.
   * @param y Top position.
   * @param w Bitmap width.
   * @param h Bitmap height.
   * @param src16 Bitmap pixels.
   */
  void drawBitmapRGB565(int x, int y, int w, int h, const uint16_t* src16);

  /**
   * @brief Pushes the complete backbuffer to the LCD panel.
   */
  void present();

private:
  static constexpr char Tag[] = "St7789Display";

  /**
   * @brief Configures the SPI bus and the LCD panel IO.
   */
  void setupSPI();

  /**
   * @brief Creates and initialises the LCD panel controller.
   */
  void setupPanel();

  int _mosi;                               /**< SPI MOSI pin. */
  int _sclk;                               /**< SPI clock pin. */
  int _cs;                                 /**< LCD chip-select pin. */
  int _dc;                                 /**< LCD data/command pin. */
  int _rst;                                /**< LCD reset pin. */
  int _bl;                                 /**< Backlight pin. */
  spi_host_device_t _host = SPI2_HOST;     /**< SPI bus used by the panel. */
  esp_lcd_panel_io_handle_t _io = nullptr; /**< LCD panel IO handle. */
  esp_lcd_panel_handle_t _panel = nullptr; /**< LCD panel handle. */

  uint16_t* backbuffer_ = nullptr; /**< WIDTH * HEIGHT RGB565 pixels. */
  size_t backbuffer_len_ = 0;      /**< Number of pixels in backbuffer_. */

  /**
   * @brief Checks whether the backbuffer is allocated.
   * @return True when drawing is possible.
   */
  inline bool hasBuffer() const {
    return backbuffer_ != nullptr;
  }
};
