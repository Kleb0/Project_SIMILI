#pragma once

#include <windows.h>

class OverlayViewport;

class CameraControl {
public:
    explicit CameraControl(OverlayViewport* overlay);
    ~CameraControl();

    void onMouseWheel(WPARAM wParam);
    void onMiddleButtonDown();
    void onMiddleButtonUp();
    void onMouseMove(WPARAM wParam, LPARAM lParam);

private:
    OverlayViewport* overlay_;
    bool is_dragging_;
    POINT last_mouse_pos_;
};
