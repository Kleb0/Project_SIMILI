#pragma once

#include <windows.h>

class OverlayViewport;
class ThreeDObjectSelector;

class RaycastPerform {
public:
    explicit RaycastPerform(OverlayViewport* overlay, ThreeDObjectSelector* selector);
    ~RaycastPerform();

    void performRaycast(int mouseX, int mouseY);

private:
    OverlayViewport* overlay_;
    ThreeDObjectSelector* selector_;
};
