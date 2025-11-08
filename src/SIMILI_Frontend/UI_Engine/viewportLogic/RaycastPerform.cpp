#include "RaycastPerform.hpp"
#include "overlay_viewport.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include "../../Engine/ThreeDScene.hpp"
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
    if (!overlay_ || !selector_) {
        std::cout << "[RaycastPerform] Raycast impossible: overlay or selector not initialized" << std::endl;
        return;
    }

    ThreeDScene* three_d_scene = overlay_->getThreeDScene();
    if (!three_d_scene) {
        std::cout << "[RaycastPerform] Raycast impossible: scene not initialized" << std::endl;
        return;
    }

    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
        std::cout << "[RaycastPerform] Click on ImGuizmo, ignoring raycast" << std::endl;
        return;
    }

    Camera* camera = three_d_scene->getActiveCamera();
    if (!camera) {
        std::cout << "[RaycastPerform] No active camera for raycast" << std::endl;
        return;
    }

    std::cout << "\n========== RAYCAST DEBUG ==========" << std::endl;
    std::cout << "[RaycastPerform] Mouse: (" << mouseX << ", " << mouseY << ")" << std::endl;

    glm::mat4 view = three_d_scene->getViewMatrix();
    glm::mat4 projection = three_d_scene->getProjectionMatrix();

    std::cout << "[RaycastPerform] Camera position: (" 
              << camera->getPosition().x << ", " 
              << camera->getPosition().y << ", " 
              << camera->getPosition().z << ")" << std::endl;

    const auto& listRef = three_d_scene->getObjectsRef();

    std::vector<ThreeDObject*> objects;
    objects.reserve(listRef.size());

    for (auto* obj : listRef) {
        if (obj) {
            std::cout << "[RaycastPerform] Object: " << obj->getName() 
                      << " | Selectable: " << (obj->isSelectable() ? "YES" : "NO")
                      << " | Pos: (" << obj->getPosition().x << ", " 
                      << obj->getPosition().y << ", " 
                      << obj->getPosition().z << ")" << std::endl;
            objects.push_back(obj);
        }
    }

    if (objects.empty()) {
        std::cout << "[RaycastPerform] ERROR: No objects in the scene" << std::endl;
        selector_->clearTarget();
        return;
    }

    int width = overlay_->getWidth();
    int height = overlay_->getHeight();

    std::cout << "[RaycastPerform] Starting raycast with " << objects.size() << " objects..." << std::endl;
    selector_->pickUpMesh(mouseX, mouseY, width, height, view, projection, objects);

    ThreeDObject* selected = selector_->getSelectedObject();

    if (selected) {
        std::cout << "\n*** OBJECT CLICKED ***" << std::endl;
        std::cout << "  Name: " << selected->getName() << std::endl;
        std::cout << "  Position: (" 
                  << selected->getPosition().x << ", " 
                  << selected->getPosition().y << ", " 
                  << selected->getPosition().z << ")" << std::endl;

        InvalidateRect(overlay_->getHandle(), nullptr, FALSE);
    } else {
        std::cout << "Clicked empty space (no object selected) - clearing selection" << std::endl;
        selector_->clearTarget();
        InvalidateRect(overlay_->getHandle(), nullptr, FALSE);
    }
    std::cout << "===================================\n" << std::endl;
}
