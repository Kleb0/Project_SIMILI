#include "UI/ObjectInspector.hpp"
#include "imgui.h"
#include <iostream>

ObjectInspector::ObjectInspector() {}

void ObjectInspector::setInspectedObject(ThreeDObject *object)
{
    inspectedObject = object;
    isRenaming = false;

    if (object)
    {
        strncpy(nameEditBuffer, object->getName().c_str(), sizeof(nameEditBuffer));
        nameEditBuffer[sizeof(nameEditBuffer) - 1] = '\0';
    }
}

void ObjectInspector::clearInspectedObject()
{
    inspectedObject = nullptr;
}

void ObjectInspector::renameObject()
{
    ImGui::Text("Object Name:");

    if (isRenaming)
    {
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::InputText("##NameEdit", nameEditBuffer, sizeof(nameEditBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            inspectedObject->setName(nameEditBuffer);
            isRenaming = false;
        }
        if (!ImGui::IsItemActive() && !ImGui::IsItemHovered())
            isRenaming = false;
    }
    else
    {
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", inspectedObject->getName().c_str());

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            isRenaming = true;
            strncpy(nameEditBuffer, inspectedObject->getName().c_str(), sizeof(nameEditBuffer));
            nameEditBuffer[sizeof(nameEditBuffer) - 1] = '\0';
        }
    }
}

void ObjectInspector::setPosition()
{
    glm::vec3 pos = inspectedObject->getPosition();
    bool modified = false;

    ImGui::Text("Position:");
    ImGui::PushItemWidth(100.0f);

    modified |= ImGui::InputFloat("X", &pos.x, 0.1f, 1.0f);
    modified |= ImGui::InputFloat("Y", &pos.y, 0.1f, 1.0f);
    modified |= ImGui::InputFloat("Z", &pos.z, 0.1f, 1.0f);

    ImGui::PopItemWidth();

    if (modified)
    {
        inspectedObject->setPosition(pos);
    }
}

void ObjectInspector::setRotation()
{
    glm::vec3 rot = inspectedObject->getRotation();
    bool modified = false;

    ImGui::Text("Rotation:");
    ImGui::PushItemWidth(100.0f);

    modified |= ImGui::InputFloat("Pitch (X)", &rot.x, 1.0f, 5.0f);
    modified |= ImGui::InputFloat("Yaw (Y)", &rot.y, 1.0f, 5.0f);
    modified |= ImGui::InputFloat("Roll (Z)", &rot.z, 1.0f, 5.0f);

    ImGui::PopItemWidth();

    if (modified)
    {
        inspectedObject->setRotation(rot);
    }
}

void ObjectInspector::setScale()
{
    glm::vec3 scale = inspectedObject->getScale();
    bool modified = false;

    ImGui::Text("Scale:");
    ImGui::PushItemWidth(100.0f);

    modified |= ImGui::InputFloat("X##scale", &scale.x, 0.1f, 0.5f);
    modified |= ImGui::InputFloat("Y##scale", &scale.y, 0.1f, 0.5f);
    modified |= ImGui::InputFloat("Z##scale", &scale.z, 0.1f, 0.5f);

    ImGui::PopItemWidth();

    if (modified)
    {
        inspectedObject->setScale(scale);
    }
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

    renameObject();

    glm::vec3 pos = inspectedObject->getPosition();

    ImGui::Separator();
    setPosition();
    ImGui::Separator();
    setRotation();
    ImGui::Separator();
    setScale();
    ImGui::End();
}