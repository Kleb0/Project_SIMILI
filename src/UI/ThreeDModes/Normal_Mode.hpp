#pragma once

#include "ThreeDMode.hpp"

class Normal_Mode : public ThreeDMode {
public:
    Normal_Mode();
    ~Normal_Mode() override = default;

    void activate(ThreeDWindow* window) override;
    void deactivate(ThreeDWindow* window) override;
    void update(ThreeDWindow* window, float deltaTime) override;
    void onKeyPress(ThreeDWindow* window, int key, int scancode, int action, int mods) override;

    const char* getName() const override;
};
