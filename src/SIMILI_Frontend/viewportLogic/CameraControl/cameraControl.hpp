#pragma once

#include <SDL3/SDL.h>

class OverlayViewport;

class CameraControl {
public:
    explicit CameraControl(OverlayViewport* overlay);
    ~CameraControl();

    void onMouseWheel(float wheel);
    void onMiddleButtonDown();
    void onMiddleButtonUp();
    void onMouseMove();
    void onZoom(int wheelDirection);

private:
    OverlayViewport* overlay_;
    bool is_dragging_;
    int last_mouse_x_;
    int last_mouse_y_;
};
