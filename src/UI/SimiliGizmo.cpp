#include <glad/glad.h>
#include "UI/SimiliGizmo.hpp"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

void SimiliGizmo::setTarget(ThreeDObject *object)
{
    target = object;
    // if (object)
    // {
    //     // std::cout << "[SIMILI_GIZMO] Target set to object at position: "
    //     //           << object->getPosition().x << ", "
    //     //           << object->getPosition().y << ", "
    //     //           << object->getPosition().z << std::endl;
    // }
}

void SimiliGizmo::initialize()
{
    if (initialized)
        return;

    // std::cout << "[SIMILI_GIZMO] Initialization complete." << std::endl;

    initialized = true;
}

void SimiliGizmo::drawInfo()
{
    if (target)
    {
        ImGui::Begin("Gizmo");
        ImGui::Text("Selected Object");
        glm::vec3 pos = target->getPosition();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        ImGui::End();
    }
}

void SimiliGizmo::activate()
{
    if (target)
    {
        // std::cout << "[SIMILI_GIZMO] activated for object at position: "
        //           << target->getPosition().x << ", "
        //           << target->getPosition().y << ", "
        //           << target->getPosition().z << std::endl;
    }
}

void SimiliGizmo::disable()
{
    target = nullptr;
    // std::cout << "[SIMILI_GIZMO] has been disabled." << std::endl;
}
