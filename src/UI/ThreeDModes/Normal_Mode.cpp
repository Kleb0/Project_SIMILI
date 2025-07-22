#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <glad/glad.h>
#include "Normal_Mode.hpp"
#include "UI/ThreeDWindow.hpp"
#include <iostream>

Normal_Mode::Normal_Mode() {}

void Normal_Mode::activate(ThreeDWindow* window) {
    std::cout << "[Mode] Normal Mode activated\n";
}

void Normal_Mode::deactivate(ThreeDWindow* window) {
    std::cout << "[Mode] Normal Mode deactivated\n";
}

void Normal_Mode::update(ThreeDWindow* window, float deltaTime) {
}

void Normal_Mode::onKeyPress(ThreeDWindow* window, int key, int scancode, int action, int mods) {
}

const char* Normal_Mode::getName() const {
    return "Normal Mode";
}
