#include "ThreeDMode.hpp"

void ThreeDMode::setMode(std::unique_ptr<ThreeDMode> newMode) {
    currentMode = std::move(newMode);
}

ThreeDMode* ThreeDMode::getCurrentMode() {
    return currentMode.get();
}
