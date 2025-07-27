#pragma once

#include "ThreeDMode.hpp"

class Vertice_Mode : public ThreeDMode {
public:
    Vertice_Mode();

    const char* getName() const override { return "Vertice Mode"; }
};
