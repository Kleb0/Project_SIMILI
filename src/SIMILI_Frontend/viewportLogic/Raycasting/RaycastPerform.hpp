#pragma once

#include <glm/glm.hpp>
#include <vector>

class ThreeDObjectSelector;
class ThreeDObject;

class RaycastPerform {
public:
    explicit RaycastPerform(ThreeDObjectSelector* selector);
    ~RaycastPerform();

    void performRaycast(
        int mouseX, int mouseY,
        int workspaceX, int workspaceY,
        int workspaceW, int workspaceH,
        const glm::mat4& view,
        const glm::mat4& projection,
        const std::vector<ThreeDObject*>& objects);

    void printRaycastDebugHeader(int mouseX, int mouseY, int viewportWidth, int viewportHeight,
        const glm::vec3& cameraPos, const std::vector<ThreeDObject*>& objects);

private:
    ThreeDObjectSelector* selector_;
};
