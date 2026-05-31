#include "WorldObjects/Camera/Camera.hpp"
#include "Engine/VulkanScene/VKScene.Hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

void Camera::initialize()
{
}

void Camera::render(const glm::mat4 &)
{
}

Camera::Camera()
{
    setPosition(glm::vec3(5.0f, 10.0f, 10.0f));
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(getPosition(), target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(fov), aspectRatio, nearClip, farClip);
}

void Camera::zoom(float offset)
{
    fov -= offset * zoomSpeed;
    if (fov < 20.0f)
        fov = 20.0f;
    if (fov > 90.0f)
        fov = 90.0f;
}

void Camera::moveForward(float amount)
{

    glm::vec3 forward = glm::normalize(target - getPosition());
    glm::vec3 newPosition = getPosition() + forward * amount;

    setPosition(newPosition);

    // std::cout << "[DEBUG] Moving camera forward: " << amount << std::endl;
}

void Camera::setResolution(int width, int height, float dpiScale)
{
    resolutionWidth_ = width;
    resolutionHeight_ = height;
    dpiScale_ = dpiScale;
    std::cout << "[Camera] Resolution updated: " << width << "x" << height << " (DPI Scale: " << dpiScale << ")" << std::endl;
}

void Camera::setVKScene(VKScene* scene)
{
    vulkanScene_ = scene;
}

void Camera::renderAttachedVKScene()
{
    if (vulkanScene_)
    {
        vulkanScene_->render(resolutionWidth_, resolutionHeight_);
    }
}


void Camera::projectSceneViewOnSDL3WorkSpace(VKScene* scene, VkCommandBuffer commandBuffer,
    int vpPixelX, int vpPixelY, int vpPixelW, int vpPixelH)
{
    if (!scene || commandBuffer == VK_NULL_HANDLE || vpPixelW <= 0 || vpPixelH <= 0)
        return;

    const float aspect = static_cast<float>(vpPixelW) / static_cast<float>(vpPixelH);
    const glm::mat4 view = getViewMatrix();
    glm::mat4 proj = getProjectionMatrix(aspect);
    proj[1][1] *= -1.0f;

    scene->drawThreeDScene(commandBuffer, vpPixelX, vpPixelY, vpPixelW, vpPixelH, view, proj);
}