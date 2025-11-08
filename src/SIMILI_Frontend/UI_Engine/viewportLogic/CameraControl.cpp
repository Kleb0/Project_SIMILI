#include "CameraControl.hpp"
#include "overlay_viewport.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"

CameraControl::CameraControl(OverlayViewport* overlay)
    : overlay_(overlay), is_dragging_(false)
{
    last_mouse_pos_.x = 0;
    last_mouse_pos_.y = 0;
}

CameraControl::~CameraControl() {}

void CameraControl::onMouseWheel(WPARAM wParam)
{
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    float wheel = delta / 120.0f;
    if (!overlay_) return;
    ThreeDScene* scene = overlay_->getThreeDScene();
    if (scene) {
        Camera* cam = scene->getActiveCamera();
        if (cam && cam->isSoftwareCamera()) 
        {
            cam->moveForward(wheel * 0.5f);
            InvalidateRect(overlay_->getHandle(), nullptr, FALSE);
        }
    }
}

void CameraControl::onMiddleButtonDown()
{
    if (!overlay_) return;
    SetCapture(overlay_->getHandle());
    is_dragging_ = true;
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(overlay_->getHandle(), &p);
    last_mouse_pos_ = p;
}

void CameraControl::onMiddleButtonUp()
{
    if (is_dragging_) 
    {
        is_dragging_ = false;
        ReleaseCapture();
    }
}

void CameraControl::onMouseMove(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (!overlay_ || !is_dragging_) return;
    ThreeDScene* scene = overlay_->getThreeDScene();
    if (!scene) return;
    Camera* cam = scene->getActiveCamera();
    if (!cam || !cam->isSoftwareCamera()) return;

    POINT current_pos;
    GetCursorPos(&current_pos);
    ScreenToClient(overlay_->getHandle(), &current_pos);

    float deltaX = static_cast<float>(current_pos.x - last_mouse_pos_.x);
    float deltaY = static_cast<float>(current_pos.y - last_mouse_pos_.y);

    if (deltaX != 0.0f || deltaY != 0.0f) {
        bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!shiftPressed) {
            cam->prepareOrbit();
            cam->orbitAroundTarget(deltaX, deltaY);
        } else {
            cam->lateralMovement(deltaX, deltaY);
        }
        InvalidateRect(overlay_->getHandle(), nullptr, FALSE);
    }

    last_mouse_pos_ = current_pos;
}
