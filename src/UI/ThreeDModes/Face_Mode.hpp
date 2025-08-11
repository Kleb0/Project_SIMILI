#pragma once

#include "ThreeDMode.hpp"

class Face_Mode : public ThreeDMode {
public:
    Face_Mode();

    const char* getName() const override { return "Face Mode"; }
};
