/**
 * @file Entity.hpp
 * @brief Abstract base for drawable objects in the world.
 */
#pragma once
#include <cstdint>

class St7789Display;

/**
 * @brief Abstract interface for objects that update and draw themselves.
 */
class Entity {
public:
  virtual ~Entity() = default;

  /**
   * @brief Updates the entity.
   * @param dt Time since the previous update in seconds.
   * @param screenW Screen width in pixels.
   * @param screenH Screen height in pixels.
   */
  virtual void update(float dt, int screenW, int screenH) = 0;

  /**
   * @brief Draws the entity.
   * @param d Display to draw on.
   */
  virtual void draw(St7789Display& d) const = 0;
};
