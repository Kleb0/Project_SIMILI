#include "CameraControl.hpp"
#include "../../../Engine/VulkanScene/VKScene.Hpp"
#include "../../../WorldObjects/Camera/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <cmath>

CameraControl::CameraControl()
    : scene_(nullptr), is_dragging_(false), last_mouse_x_(0), last_mouse_y_(0)
    , yaw_(-90.0f), pitch_(0.0f), orbit_radius_(10.0f), is_orbit_prepared_(false)
{
}

CameraControl::~CameraControl() {}

void CameraControl::setScene(VKScene* scene)
{
    scene_ = scene;
}

void CameraControl::onMiddleButtonDown(int mouseX, int mouseY)
{
    is_dragging_ = true;
    last_mouse_x_ = mouseX;
    last_mouse_y_ = mouseY;
}

void CameraControl::onMiddleButtonUp()
{
    is_dragging_ = false;
    resetOrbitPreparation();
}

void CameraControl::onMouseMove(int mouseX, int mouseY)
{
    if (!scene_ || !is_dragging_)
        return;

    Camera* cam = scene_->getActiveCamera();
    if (!cam || !cam->isSoftwareCamera())
        return;

    float deltaX = static_cast<float>(mouseX - last_mouse_x_);
    float deltaY = static_cast<float>(mouseY - last_mouse_y_);

    if (deltaX != 0.0f || deltaY != 0.0f)
    {
        bool shiftPressed = SDL_GetModState() & SDL_KMOD_SHIFT;
        if (!shiftPressed)
        {
            prepareOrbit(cam);
            orbitAroundTarget(cam, deltaX, deltaY);
        }
        else
        {
            lateralMovement(cam, deltaX, deltaY);
        }
    }

    last_mouse_x_ = mouseX;
    last_mouse_y_ = mouseY;
}

void CameraControl::onWheel(float wheelDelta)
{
    if (!scene_)
        return;

    Camera* cam = scene_->getActiveCamera();
    if (!cam || !cam->isSoftwareCamera())
        return;

    cam->zoom(wheelDelta);
}

void CameraControl::prepareOrbit(Camera* cam)
{
    if (is_orbit_prepared_)
        return;

    glm::vec3 offset = cam->getPosition() - cam->target;
    orbit_radius_ = glm::length(offset);

    pitch_ = glm::degrees(asin(offset.y / orbit_radius_));
    yaw_ = glm::degrees(atan2(offset.z, offset.x));

    is_orbit_prepared_ = true;
}

void CameraControl::resetOrbitPreparation()
{
    is_orbit_prepared_ = false;
}

void CameraControl::orbitAroundTarget(Camera* cam, float deltaX, float deltaY)
{
    const float sensitivity = 0.1f;

    yaw_ += deltaX * sensitivity;
    pitch_ -= deltaY * sensitivity;

    if (pitch_ > 89.0f)
        pitch_ = 89.0f;
    if (pitch_ < -89.0f)
        pitch_ = -89.0f;

    float radYaw = glm::radians(yaw_);
    float radPitch = glm::radians(pitch_);

    glm::vec3 direction;
    direction.x = orbit_radius_ * cos(radPitch) * cos(radYaw);
    direction.y = orbit_radius_ * sin(radPitch);
    direction.z = orbit_radius_ * cos(radPitch) * sin(radYaw);

    cam->setPosition(cam->target + direction);
}

void CameraControl::lateralMovement(Camera* cam, float deltaX, float deltaY)
{
    const float sensitivity = 0.005f;

    glm::vec3 forward = glm::normalize(cam->target - cam->getPosition());
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    glm::vec3 translation = (-right * deltaX + up * deltaY) * sensitivity;

    cam->setPosition(cam->getPosition() + translation);
    cam->target += translation;
}

