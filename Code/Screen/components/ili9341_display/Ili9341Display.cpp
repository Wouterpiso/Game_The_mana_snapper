/*! \brief Implementation for Ili9341 Display wrapper
 *
 *  This is a demonstration of a more game-loop-ready implementation of the
 *  display wrapper
 *
 */
#include "Ili9341Display.hpp"

#include <algorithm>  // std::max, std::min
#include <cstdlib>    // std::abs
#include <cstring>    // std::memcpy

#include "esp_log.h"

// REQUIRED for esp_lcd_panel_io_spi_config_t and related functions
#include "esp_lcd_panel_interface.h"  // full definition of esp_lcd_panel_t
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_st7789.h"

// The ILI9341 expects RGB565 with the high byte first, but colors are stored
// natively (little endian) in the backbuffer, so swap on every pixel write.
static inline uint16_t panelColor(uint16_t color) {
  return static_cast<uint16_t>((color >> 8) | (color << 8));
}

Ili9341Display::Ili9341Display(int mosi, int sclk, int cs, int dc, int rst, int bl)
    : _mosi(mosi), _sclk(sclk), _cs(cs), _dc(dc), _rst(rst), _bl(bl) {}

Ili9341Display::~Ili9341Display() {
  if (backbuffer_) {
    heap_caps_free(backbuffer_);
    backbuffer_ = nullptr;
    backbuffer_len_ = 0;
  }
  backlightOff();
}

void Ili9341Display::init() {
  setupSPI();
  setupPanel();
  // Allocate full-screen backbuffer
  backbuffer_len_ = WIDTH * HEIGHT;
  size_t sz_buffer = backbuffer_len_ * sizeof(uint16_t);
  backbuffer_ =
      static_cast<uint16_t*>(heap_caps_malloc(sz_buffer, MALLOC_CAP_DMA));
  if (!backbuffer_) {
    ESP_LOGE(TAG, "Failed to allocate %d-byte backbuffer", (int)(sz_buffer));
  }
  backlightOn();
}

void Ili9341Display::setupSPI()
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = _mosi,
        .miso_io_num = -1,
        .sclk_io_num = _sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .data_io_default_level = false,
        .max_transfer_sz = WIDTH * HEIGHT * 2 + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO,
        .intr_flags = 0
    };

    ESP_ERROR_CHECK(spi_bus_initialize(_host, &buscfg, SPI_DMA_CH_AUTO));
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = _cs,
        .dc_gpio_num = _dc,
        .spi_mode = 0,
        .pclk_hz = 30 * 1000 * 1000, // 30 MHz
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .flags = {
            .dc_high_on_cmd = 0,
            .dc_low_on_data = 0,
            .dc_low_on_param = 0,
            .octal_mode = 0,
            .quad_mode = 0,
            .sio_mode = 0,
            .lsb_first = 0,
            .cs_high_active = 0
        }
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(_host, &io_config, &_io));
}

void Ili9341Display::setupPanel() {

// The TPM408-2.8 (Cheap Yellow Display) ships with either an ILI9341 or an
// ST7789 controller depending on the batch; the box label "7789" marks the
// ST7789 units. Running ST7789 silicon with the ILI9341 init/gamma gives a
// washed out image with lifted blacks, so the driver is selected here.
#define PANEL_ST7789

#if defined(PANEL_ST7789)
  const bool SWAP_AXES = true;
  const bool MIRROR_X = true;
  const bool MIRROR_Y = false;
  const bool INVERT_COLOR = false;
  const auto LCD_ELEMENT_ORDER = LCD_RGB_ELEMENT_ORDER_RGB;
#elif defined(DISPLAY28) // 2.8 inch display
  const bool SWAP_AXES = true;
  const bool MIRROR_X = true;
  const bool MIRROR_Y = false;
  const bool INVERT_COLOR = false;
  const auto LCD_ELEMENT_ORDER = LCD_RGB_ELEMENT_ORDER_RGB;
#else // 2.4 inch display
  const bool SWAP_AXES = true;
  const bool MIRROR_X = false;
  const bool MIRROR_Y = true;
  const bool INVERT_COLOR = false;
  const auto LCD_ELEMENT_ORDER = LCD_RGB_ELEMENT_ORDER_RGB;
#endif

  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = _rst,
      .rgb_ele_order = LCD_ELEMENT_ORDER,
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG, //changing this seems to have no effect
      .bits_per_pixel = 16,
      .flags = {.reset_active_high = 0},
      .vendor_config = NULL};

    // Create raw panel object without auto init
#if defined(PANEL_ST7789)
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(_io, &panel_config, &_panel));
#else
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(_io, &panel_config, &_panel));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(_panel));

    // Landscape, origin top-left, invert colors if necessary
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(_panel, SWAP_AXES));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(_panel, MIRROR_X, MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(_panel, INVERT_COLOR));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(_panel, true));
}

/*! \brief convert 24-bit RGB into 16-bit RGB565 format
 *
 *  RGB565 is commonly used in LCD displays
 */
uint16_t Ili9341Display::rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint16_t>(((r & 0xF8) << 8) |
                                 ((g & 0xFC) << 3) |
                                 ((b & 0xF8) >> 3));
}

/// @brief Push the current backbuffer (frame) to the LCD panel
void Ili9341Display::present() {
  if (!_panel) {
    ESP_LOGE(TAG, "present: _panel is NULL");
    return;
  }
  // Trasnfer the backbuffer to the panel
  esp_err_t err =
      esp_lcd_panel_draw_bitmap(_panel, 0, 0, WIDTH, HEIGHT, backbuffer_);
  // slight delay before new draw transfer is done.
  // ToDo: implement a callback
  vTaskDelay(pdMS_TO_TICKS(41));
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "present: failed to draw bitmap: %s", esp_err_to_name(err));
  }
}

/// @brief Fill the screen buffer with a single color
void Ili9341Display::fillScreen(uint16_t color) {
  if (!hasBuffer()) {
    ESP_LOGE(TAG, "fillScreen: backbuffer not allocated");
    return;
  }
  // Fill the backbuffer
  for (int i = 0; i < (int)backbuffer_len_; ++i) {
    backbuffer_[i] = panelColor(color);
  }
}

void Ili9341Display::backlightOn() {
  if (_bl < 0) {
    return;  // pin not defined -> do nothing
  }

  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << _bl),
                           .mode = GPIO_MODE_OUTPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);
  gpio_set_level(static_cast<gpio_num_t>(_bl), 1);
}

void Ili9341Display::backlightOff() {
  if (_bl < 0) {
    return;  // pin not defined -> do nothing
  }

  gpio_set_level(static_cast<gpio_num_t>(_bl), 0);
}

/*! \brief start of Drawing Primitives
 *
 *  drawPixel() - Access panel through backbuffer
 *  drawLine() - Bresenham's Line Algorithm
 *  drawRectangle() - Draw 4 lines or fill a region
 *  drawCircle() - Bresenham's Circle Algorithm for a hollow circle, Euclid for
 * a filled one drawTriangle() - Draw 3 lines or fill using scanlines
 */
void Ili9341Display::drawPixel(int x, int y, uint16_t color) {
  if ((unsigned)x >= (unsigned)WIDTH || (unsigned)y >= (unsigned)HEIGHT) {
    ESP_LOGE(TAG, "drawPixel: out of bounds x=%d, y=%d", x, y);
    return;  // discard
  }
  // no fallback implemented
  // but you could directly draw to the panel (like in previous examples)
  if (!hasBuffer()) {
    ESP_LOGE(TAG, "drawPixel: backbuffer not allocated, cannot perform draw");
    return;
  }
  // draw into backbuffer
  backbuffer_[y * WIDTH + x] = panelColor(color);
}

void Ili9341Display::drawLine(int x0, int y0, int x1, int y1, uint16_t color) {
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;

  while (true) {
    drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) {
      break;
    }

    const int error2 = 2 * error;
    if (error2 >= dy) {
      error += dy;
      x0 += sx;
    }
    if (error2 <= dx) {
      error += dx;
      y0 += sy;
    }
  }
}

void Ili9341Display::drawRectangle(int x, int y, int w, int h, uint16_t color,
                                   bool fill) {
  if (fill) {
    for (int o = 0; o <= h; o++) {
      drawLine(x, y + o, x + w, y + o, color);
    }
  } else {
    drawLine(x, y, x + w, y, color);          // right
    drawLine(x + w, y, x + w, y + h, color);  // down
    drawLine(x + w, y + h, x, y + h, color);  // left
    drawLine(x, y + h, x, y, color);          // up
  }
}

void Ili9341Display::drawTriangle(int x0, int y0, int x1, int y1, int x2,
                                  int y2, uint16_t color, bool fill) {
  if (fill) {
    // TODO: PLEASE IMPLEMENT THIS FUNCTION
  } else {
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
  }
}

/*! \brief Draw circle with Bresenham's Circle Algorithm.
 *
 *  https://en.wikipedia.org/wiki/Midpoint_circle_algorithm
 * 
 * Draw a filled circle with a simple Euclid computation.
 *
 */
void Ili9341Display::drawCircle(int x0, int y0, int r, uint16_t color, bool fill) {
  int cx = x0;  // x0 - r; // if you want to set the center point
  int cy = y0;  // y0 - r; // if you want to set the center point

  if (fill) {
    for (int y = 0; y <= r; y++) {
      for (int x = 0; x <= r; x++) {
        if (x * x + y * y <= r * r) {
          // plot points following 4-fold symmetry
          drawPixel(cx + x, cy + y, color);
          drawPixel(cx - x, cy + y, color);
          drawPixel(cx + x, cy - y, color);
          drawPixel(cx - x, cy - y, color);
        }
      }
    }
  } else {
    // TODO: PLEASE IMPLEMENT THIS FUNCTION
  }
}
// end of Drawing Primitives

// Drawing bitmaps
void Ili9341Display::drawBitmapRGB565(int x, int y, int w, int h, const uint8_t* src) {
  if (!hasBuffer()) {
    ESP_LOGE(TAG, "drawBitmapRGB565: backbuffer not allocated");
    return;
  }

  // Trivial reject if invalid parameters or completely off-screen
  if (!src || w <= 0 || h <= 0) return;
  if (x >= WIDTH || y >= HEIGHT || x + w <= 0 || y + h <= 0) return;

  // Clip to screen
  const int x0 = std::max(0, x);
  const int y0 = std::max(0, y);
  const int x1 = std::min(WIDTH, x + w);
  const int y1 = std::min(HEIGHT, y + h);
  const int copyW = x1 - x0;
  const int copyH = y1 - y0;
  if (copyW <= 0 || copyH <= 0) return;

  // Starting offset in the source (account for clipped left/top)
  // Source stride is exactly w pixels per row, 2 bytes per pixel
  const int srcSkipX = x0 - x;  // pixels to skip at left
  const int srcSkipY = y0 - y;  // rows to skip at top
  const size_t srcRowBytes = static_cast<size_t>(w) * 2;
  const size_t dstRowPixels = WIDTH;

  for (int row = 0; row < copyH; ++row) {
    const int dstY = y0 + row;
    const int dstX = x0;

    // Compute destination pointer (uint16_t backbuffer in RGB565)
    uint16_t* dst = backbuffer_ + static_cast<size_t>(dstY) * dstRowPixels + dstX;

    // Compute source pointer (byte stream, little-endian RGB565)
    const uint8_t* srow = src
                          + (static_cast<size_t>(srcSkipY + row) * srcRowBytes)
                          + (static_cast<size_t>(srcSkipX) * 2);

    // Copy one clipped scanline, swapping to the panel's byte order
    for (int column = 0; column < copyW; ++column) {
      dst[column] = static_cast<uint16_t>((srow[column * 2] << 8) |
                                          srow[column * 2 + 1]);
    }
  }
}

void Ili9341Display::drawBitmapRGB565(int x, int y, int w, int h, const uint16_t* src16) {
  // Thin wrapper: forward to the byte-stream version without aliasing issues
  drawBitmapRGB565(x, y, w, h, reinterpret_cast<const uint8_t*>(src16));
}

// EOF