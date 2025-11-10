#pragma once
#include <imgui.h>
#include <ImGuizmo.h>

class OverlayViewport;
class ThreeDScene;

class OverlayClickHandler {
public:
    explicit OverlayClickHandler(OverlayViewport* owner);
    void handle();

private:
    OverlayViewport* viewport;
    ThreeDScene* scene;
};
