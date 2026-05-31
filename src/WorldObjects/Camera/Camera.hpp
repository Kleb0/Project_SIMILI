#pragma once

#include <string>
#include <vulkan/vulkan.h>
#include "WorldObjects/Entities/ThreedObject.hpp"

class VKScene;
class ThreeDScreen;

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
    }

    glm::vec3 getTarget() const { return target; }

    bool isCurrentUserCamera() const { return true; }
    bool isSoftwareCamera() const { return true; }
    bool isAGameCamera() const { return false; }
    bool isSelectable() const override { return !isCurrentUserCamera(); }
    void zoom(float offset);
    void moveForward(float amount);

    void setResolution(int width, int height, float dpiScale = 1.0f);
    int getResolutionWidth() const { return resolutionWidth_; }
    int getResolutionHeight() const { return resolutionHeight_; }
    float getDpiScale() const { return dpiScale_; }

    void setVKScene(VKScene* scene);
    void renderAttachedVKScene();
    VKScene* getVulkanScene() const { return vulkanScene_; }
    void projectSceneViewOnSDL3WorkSpace(VKScene* scene, VkCommandBuffer commandBuffer,
        int vpPixelX, int vpPixelY, int vpPixelW, int vpPixelH);

    float fov = 45.0f;
    float nearClip = 0.1f;
    float farClip = 100.0f;
    float zoomSpeed = 2.5f;
    float zommFactor = 1.0f;

private:
    int resolutionWidth_ = 800;
    int resolutionHeight_ = 600;
    float dpiScale_ = 1.0f;

    VKScene* vulkanScene_ = nullptr;
};
