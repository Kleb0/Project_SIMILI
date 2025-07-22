#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <glad/glad.h>

#include "Vertice_Mode.hpp"
#include "UI/ThreeDWindow.hpp"
#include <iostream>

Vertice_Mode::Vertice_Mode() {}

void Vertice_Mode::activate(ThreeDWindow* window) {
    std::cout << "[Mode] Vertice Mode activated\n";
}

void Vertice_Mode::deactivate(ThreeDWindow* window) {
    std::cout << "[Mode] Vertice Mode deactivated\n";
}

void Vertice_Mode::update(ThreeDWindow* window, float deltaTime) {
}

void Vertice_Mode::onKeyPress(ThreeDWindow* window, int key, int scancode, int action, int mods) {
}

const char* Vertice_Mode::getName() const {
    return "Vertice Mode";
}
