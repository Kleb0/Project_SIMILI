#pragma once

#include "ThreeDMode.hpp"

class Normal_Mode : public ThreeDMode {
public:
    Normal_Mode();

    const char* getName() const override { return "Normal Mode"; }
};
