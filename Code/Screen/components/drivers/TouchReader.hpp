/**
 * @file TouchReader.hpp
 * @brief XPT2046 touch controller driver.
 */
#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

/**
 * @brief Reads touch points from the XPT2046 controller.
 *
 * The controller is polled through its IRQ pin; points are scaled from raw
 * ADC values to the 320x240 screen coordinates.
 */
class TouchReader {
public:
  /**
   * @brief One touch point in screen coordinates.
   */
  struct Point {
    int x; /**< Screen x coordinate. */
    int y; /**< Screen y coordinate. */
  };

  /**
   * @brief Creates the touch reader and its SPI device.
   * @param host SPI bus to use.
   * @param clk SPI clock pin.
   * @param mosi SPI MOSI pin.
   * @param miso SPI MISO pin.
   * @param cs Touch chip-select pin.
   * @param irq Touch interrupt pin.
   */
  TouchReader(spi_host_device_t host, gpio_num_t clk, gpio_num_t mosi, gpio_num_t miso,
              gpio_num_t cs, gpio_num_t irq);

  /**
   * @brief Removes the SPI device and releases the bus.
   */
  ~TouchReader();

  /**
   * @brief Configures the interrupt pin as an input.
   */
  void start();

  /**
   * @brief Checks whether the screen is being touched.
   * @return True while a touch is detected.
   */
  bool touched() const;

  /**
   * @brief Reads the current touch point.
   * @return Point in screen coordinates, clamped to the screen.
   */
  Point readPoint() const;

  /**
   * @brief Waits until the user lifts the finger.
   */
  void waitForRelease() const;

private:
  spi_host_device_t host_;               /**< SPI bus used by the controller. */
  gpio_num_t irq_;                       /**< Touch interrupt pin. */
  spi_device_handle_t device_ = nullptr; /**< SPI device handle. */
};
