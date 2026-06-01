#include "RaycastPerform.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "../../WorldObjects/Entities/ThreeDObject.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

RaycastPerform::RaycastPerform(ThreeDObjectSelector* selector)
    : selector_(selector)
{
}

RaycastPerform::~RaycastPerform() {}

void RaycastPerform::printRaycastDebugHeader(int mouseX, int mouseY, int viewportWidth, int viewportHeight,
    const glm::vec3& cameraPos, const std::vector<ThreeDObject*>& objects)
{
    std::cout << "\n ========== RAYCAST DEBUG ==========" << std::endl;
    std::cout << "[RaycastPerform] Mouse: (" << mouseX << ", " << mouseY << ")" << std::endl;
    std::cout << "[RaycastPerform] Viewport Resolution: " << viewportWidth << "x" << viewportHeight << std::endl;
    std::cout << "[RaycastPerform] Camera position: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;

    for (const auto* obj : objects) {
        if (!obj) continue;
        std::cout << "[RaycastPerform] Object: " << obj->getName()
                  << " | Selectable: " << (obj->isSelectable() ? "YES" : "NO")
                  << " | Pos: (" << obj->getPosition().x << ", " << obj->getPosition().y << ", " << obj->getPosition().z << ")" << std::endl;
    }

    std::cout << "[RaycastPerform] Starting raycast with " << objects.size() << " objects..." << std::endl;
}

void RaycastPerform::performRaycast(
    int mouseX, int mouseY,
    int workspaceX, int workspaceY,
    int workspaceW, int workspaceH,
    const glm::mat4& view,
    const glm::mat4& projection,
    const std::vector<ThreeDObject*>& objects)
{
    if (!selector_)
        return;

    if (workspaceW <= 0 || workspaceH <= 0)
        return;

    int localX = mouseX - workspaceX;
    int localY = mouseY - workspaceY;

    if (localX < 0 || localY < 0 || localX > workspaceW || localY > workspaceH)
        return;

    selector_->pickUpMesh(localX, localY, workspaceW, workspaceH, view, projection, objects);

    ThreeDObject* hit = selector_->getSelectedObject();
    if (hit)
        std::cout << "[RaycastPerform] Hit: " << hit->getName() << std::endl;
    else
        std::cout << "[RaycastPerform] Aucun objet touche" << std::endl;
}
