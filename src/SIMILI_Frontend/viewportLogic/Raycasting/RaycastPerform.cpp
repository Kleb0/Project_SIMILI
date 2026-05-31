#include "RaycastPerform.hpp"
// #include "overlay_viewport.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include "../../Engine/OpenGLScene/ThreeDScene.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Entities/ThreeDObject.hpp"
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

RaycastPerform::RaycastPerform(OverlayViewport* overlay, ThreeDObjectSelector* selector)
    : overlay_(overlay), selector_(selector)
{
}

RaycastPerform::~RaycastPerform() {}

void RaycastPerform::performRaycast(int mouseX, int mouseY)
{

}
