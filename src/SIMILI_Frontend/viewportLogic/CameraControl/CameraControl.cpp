#include "CameraControl.hpp"
#include "../overlay_viewport.hpp"
#include "../../Engine/VulkanScene/VKScene.Hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include <SDL3/SDL.h>

CameraControl::CameraControl(OverlayViewport* overlay)
    : overlay_(overlay), is_dragging_(false), last_mouse_x_(0), last_mouse_y_(0)
{
}

CameraControl::~CameraControl() {}

void CameraControl::onMouseWheel(float wheel)
{
    if (!overlay_) return;
    VKScene* scene = overlay_->getVKScene();
    if (scene) {
        Camera* cam = scene->getActiveCamera();
        if (cam && cam->isSoftwareCamera()) 
        {
            cam->moveForward(wheel * 0.5f);
        }
    }
}

void CameraControl::onMiddleButtonDown()
{
    if (!overlay_) return;
    is_dragging_ = true;
    float mouseXfloat, mouseYfloat;
    SDL_GetMouseState(&mouseXfloat, &mouseYfloat);
    last_mouse_x_ = static_cast<int>(mouseXfloat);
    last_mouse_y_ = static_cast<int>(mouseYfloat);
}

void CameraControl::onMiddleButtonUp()
{
    if (is_dragging_) 
    {
        is_dragging_ = false;
    }
}

void CameraControl::onMouseMove()
{
    if (!overlay_ || !is_dragging_) return;
    VKScene* scene = overlay_->getVKScene();
    if (!scene) return;
    Camera* cam = scene->getActiveCamera();
    if (!cam || !cam->isSoftwareCamera()) return;

    float mouseXfloat, mouseYfloat;
    SDL_GetMouseState(&mouseXfloat, &mouseYfloat);
    int current_x = static_cast<int>(mouseXfloat);
    int current_y = static_cast<int>(mouseYfloat);

    float deltaX = static_cast<float>(current_x - last_mouse_x_);
    float deltaY = static_cast<float>(current_y - last_mouse_y_);

    if (deltaX != 0.0f || deltaY != 0.0f) {
        bool shiftPressed = SDL_GetModState() & SDL_KMOD_SHIFT;
        if (!shiftPressed) {
            cam->prepareOrbit();
            cam->orbitAroundTarget(deltaX, deltaY);
        } else {
            cam->lateralMovement(deltaX, deltaY);
        }
    }

    last_mouse_x_ = current_x;
    last_mouse_y_ = current_y;
}

void CameraControl::onZoom(int wheelDirection)
{
    if (!overlay_) return;
    VKScene* scene = overlay_->getVKScene();
    if (!scene) return;
    Camera* cam = scene->getActiveCamera();
    if (!cam || !cam->isSoftwareCamera()) return;

    cam->zoom(static_cast<float>(wheelDirection));
}
