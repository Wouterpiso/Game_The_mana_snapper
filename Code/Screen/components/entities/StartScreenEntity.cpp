/**
 * @file StartScreenEntity.cpp
 * @brief Implements the boot logo drawing.
 */
#include "StartScreenEntity.hpp"

#include "St7789Display.hpp"
#include "Logo.hpp"

#include <cstring>

void StartScreenEntity::draw(St7789Display& display) const {
  constexpr int height = 220;
  constexpr int width = (IMAGE_HEIGHT * height) / IMAGE_WIDTH;
  const int left = (St7789Display::WIDTH - width) / 2;
  const int top = (St7789Display::HEIGHT - height) / 2;

  for (int y = 0; y < height; ++y) {
    const int sourceX = IMAGE_WIDTH - 1 - (y * (IMAGE_WIDTH - 1)) / (height - 1);
    for (int x = 0; x < width; ++x) {
      const int sourceY = (x * (IMAGE_HEIGHT - 1)) / (width - 1);
      uint16_t pixel = 0;
      std::memcpy(&pixel, the_mana_snapper_map + (sourceY * IMAGE_WIDTH + sourceX) * sizeof(pixel),
                  sizeof(pixel));
      if (pixel != 0) {
        display.drawPixel(left + x, top + y, pixel);
      }
    }
  }
}
