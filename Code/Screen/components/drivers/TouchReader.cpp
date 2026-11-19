/**
 * @file TouchReader.cpp
 * @brief Implements the XPT2046 touch controller driver.
 */
#include "TouchReader.hpp"

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TouchReader::TouchReader(spi_host_device_t host, gpio_num_t clk, gpio_num_t mosi, gpio_num_t miso,
                         gpio_num_t cs, gpio_num_t irq)
    : host_(host), irq_(irq) {
  spi_bus_config_t bus_config = {};
  bus_config.sclk_io_num = clk;
  bus_config.mosi_io_num = mosi;
  bus_config.miso_io_num = miso;
  bus_config.quadwp_io_num = -1;
  bus_config.quadhd_io_num = -1;
  bus_config.max_transfer_sz = 4;
  ESP_ERROR_CHECK(spi_bus_initialize(host_, &bus_config, SPI_DMA_DISABLED));

  spi_device_interface_config_t device_config = {};
  device_config.clock_speed_hz = 2 * 1000 * 1000;
  device_config.mode = 0;
  device_config.spics_io_num = cs;
  device_config.queue_size = 1;
  ESP_ERROR_CHECK(spi_bus_add_device(host_, &device_config, &device_));
}

TouchReader::~TouchReader() {
  if (device_ != nullptr) {
    spi_bus_remove_device(device_);
  }
  spi_bus_free(host_);
}

void TouchReader::start() {
  gpio_config_t config = {};
  config.pin_bit_mask = 1ULL << irq_;
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_DISABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  ESP_ERROR_CHECK(gpio_config(&config));
}

bool TouchReader::touched() const {
  return gpio_get_level(irq_) == 0;
}

TouchReader::Point TouchReader::readPoint() const {
  auto readAxis = [this](uint8_t command) {
    spi_transaction_t transaction = {};
    uint8_t tx[3] = {command, 0, 0};
    uint8_t rx[3] = {};
    transaction.length = 24;
    transaction.tx_buffer = tx;
    transaction.rx_buffer = rx;
    ESP_ERROR_CHECK(spi_device_transmit(device_, &transaction));
    return static_cast<int>((((rx[1] << 8) | rx[2]) >> 3) & 0x0FFF);
  };

  constexpr int minRaw = 200;
  constexpr int maxRaw = 3800;
  const int rawX = readAxis(0xD0);
  const int rawY = readAxis(0x90);
  const int x = (rawX - minRaw) * 320 / (maxRaw - minRaw);
  const int y = (rawY - minRaw) * 240 / (maxRaw - minRaw);
  return {x < 0 ? 0 : (x > 319 ? 319 : x), y < 0 ? 0 : (y > 239 ? 239 : y)};
}

void TouchReader::waitForRelease() const {
  while (touched()) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
