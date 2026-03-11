#pragma once
#include <imgui.h>
#include <ImGuizmo.h>

class OverlayViewport;
class VKScene;

class OverlayClickHandler {
public:
    explicit OverlayClickHandler(OverlayViewport* owner);
    void handle();

private:
    OverlayViewport* viewport;
    VKScene* scene;
};
