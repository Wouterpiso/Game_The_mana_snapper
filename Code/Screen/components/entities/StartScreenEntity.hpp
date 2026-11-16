#pragma once

#include "Entity.hpp"

class StartScreenEntity final : public Entity {
public:
    static constexpr int IMAGE_WIDTH = 2656;
    static constexpr int IMAGE_HEIGHT = 512;

    void update(float, int, int) override {}
    void draw(Ili9341Display& display) const override;
};
