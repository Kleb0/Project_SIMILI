#include "WorldObjects/Camera.hpp"
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
    std::cout << "[DEBUG] Zooming camera: " << offset << std::endl;
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

    orbitRadius = glm::length(target - newPosition);

    // std::cout << "[DEBUG] Moving camera forward: " << amount << std::endl;
}

void Camera::prepareOrbit()
{
    if (isOrbitPrepared)
        return;

    glm::vec3 offset = getPosition() - target;
    orbitRadius = glm::length(offset);

    pitch = glm::degrees(asin(offset.y / orbitRadius));
    yaw = glm::degrees(atan2(offset.z, offset.x));

    isOrbitPrepared = true;
}

void Camera::resetOrbitPreparation()
{
    isOrbitPrepared = false;
}

void Camera::orbitAroundTarget(float deltaX, float deltaY)
{
    const float sensitivity = 0.1f;

    yaw += deltaX * sensitivity;
    pitch -= deltaY * sensitivity;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    float radYaw = glm::radians(yaw);
    float radPitch = glm::radians(pitch);

    glm::vec3 direction;
    direction.x = orbitRadius * cos(radPitch) * cos(radYaw);
    direction.y = orbitRadius * sin(radPitch);
    direction.z = orbitRadius * cos(radPitch) * sin(radYaw);
    setPosition(target + direction);
}

void Camera::lateralMovement(float deltaX, float deltaY)
{
    const float sensitivity = 0.005f;

    glm::vec3 forward = glm::normalize(target - getPosition());
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    glm::vec3 translation = (-right * deltaX + up * deltaY) * sensitivity;

    setPosition(getPosition() + translation);
    target += translation;
}