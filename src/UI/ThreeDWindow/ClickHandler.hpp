#pragma once
#include <imgui.h>
#include <ImGuizmo.h>


class ThreeDWindow;
class ThreeDScene;

class ClickHandler {
public:
    explicit ClickHandler(ThreeDWindow* owner);
    void handle();

private:
    ThreeDWindow* window;
    ThreeDScene* scene;
};
