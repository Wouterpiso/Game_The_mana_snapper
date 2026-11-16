#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

class TouchReader {
public:
    struct Point {
        int x;
        int y;
    };

    TouchReader(spi_host_device_t host, gpio_num_t clk, gpio_num_t mosi,
                gpio_num_t miso, gpio_num_t cs, gpio_num_t irq);
    ~TouchReader();

    void start();
    bool touched() const;
    Point readPoint() const;
    void waitForRelease() const;

private:
    spi_host_device_t host_;
    gpio_num_t irq_;
    spi_device_handle_t device_ = nullptr;
};
