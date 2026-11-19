/**
 * @file Sprites.hpp
 * @brief Minimal sprite description shared by renderers.
 */
#pragma once
#include <cstdint>

namespace sprites {

/**
 * @brief Plain sprite description.
 *
 * @c pixels is an RGB565 byte stream (little-endian), row-major, two bytes per
 * pixel.
 */
struct Sprite {
  uint16_t w;            /**< Sprite width in pixels. */
  uint16_t h;            /**< Sprite height in pixels. */
  const uint8_t* pixels; /**< RGB565 pixels, little-endian, row-major. */
};

} // namespace sprites
