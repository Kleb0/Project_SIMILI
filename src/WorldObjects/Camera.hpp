#pragma once

#include <string>
#include "WorldObjects/ThreedObject.hpp"

class Camera : public ThreeDObject
{
public:
    Camera();
    void initialize() override;
    void render(const glm::mat4 &viewProj) override;
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;
    std::string getName() const { return "MainCamera"; }

    glm::vec3 target = glm::vec3(2.5f, 0.0f, 2.5f);

    void setTarget(const glm::vec3 &newTarget)
    {
        target = newTarget;
        orbitRadius = glm::length(getPosition() - target);
    }

    glm::vec3 getTarget() const { return target; }

    bool isCurrentUserCamera() const { return true; }
    bool isSoftwareCamera() const { return true; }
    bool isAGameCamera() const { return false; }
    bool isSelectable() const override { return !isCurrentUserCamera(); }
    bool isOrbitPrepared = false;

    void zoom(float offset);
    void moveForward(float amount);
    void prepareOrbit();
    void resetOrbitPreparation();
    void orbitAroundTarget(float deltaX, float deltaY);
    void lateralMovement(float deltaX, float deltaY);

    float fov = 45.0f;
    float nearClip = 0.1f;
    float farClip = 100.0f;
    float zoomSpeed = 0.1f;
    float zommFactor = 1.0f;

private:
    float yaw = -90.0f;
    float pitch = 0.0f;
    float orbitRadius = 10.0f;
};
