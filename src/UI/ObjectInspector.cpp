#include "UI/ObjectInspector.hpp"
#include "imgui.h"
#include <iostream>

ObjectInspector::ObjectInspector() {}

void ObjectInspector::setInspectedObject(ThreeDObject *object)
{
    inspectedObject = object;
}

void ObjectInspector::clearInspectedObject()
{
    inspectedObject = nullptr;
}

void ObjectInspector::render()
{
    ImGui::Begin("Object Inspector");

    if (!inspectedObject)
    {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    ImGui::Text("Object Name:");
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", inspectedObject->getName().c_str());

    glm::vec3 pos = inspectedObject->getPosition();

    ImGui::Separator();
    ImGui::Text("Position:");
    ImGui::BeginDisabled();
    ImGui::InputFloat("X", &pos.x, 0.1f, 1.0f);
    ImGui::InputFloat("Y", &pos.y, 0.1f, 1.0f);
    ImGui::InputFloat("Z", &pos.z, 0.1f, 1.0f);
    ImGui::EndDisabled();

    ImGui::End();
}