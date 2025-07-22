#pragma once

#include "ThreeDMode.hpp"
#include <iostream>

class Vertice_Mode : public ThreeDMode {
public:
    void activate(ThreeDWindow* window) override {
        std::cout << "[Mode] Vertice Mode activated\n";
    }

    void deactivate(ThreeDWindow* window) override {
        std::cout << "[Mode] Vertice Mode deactivated\n";
    }

    void update(ThreeDWindow* window, float deltaTime) override {
    }

    const char* getName() const override {
        return "Vertice Mode";
    }
};
