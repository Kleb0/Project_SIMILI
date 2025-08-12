#pragma once

#include "ThreeDMode.hpp"

class Edge_Mode : public ThreeDMode {
public:
    Edge_Mode();

    const char* getName() const override { return "Edge Mode"; }
};
