/**
 * @file StartScreenEntity.hpp
 * @brief Entity that draws the boot logo.
 */
#pragma once

#include "Entity.hpp"

/**
 * @brief Draws the rotated ManaSnapper logo on the start screen.
 */
class StartScreenEntity final : public Entity {
public:
  static constexpr int IMAGE_WIDTH = 2656; /**< Logo width in pixels. */
  static constexpr int IMAGE_HEIGHT = 512; /**< Logo height in pixels. */

  /**
   * @brief The logo is static; it needs no per-frame update.
   */
  void update(float, int, int) override {}

  /**
   * @brief Draws the logo centred on the display.
   * @param display Display to draw on.
   */
  void draw(St7789Display& display) const override;
};
