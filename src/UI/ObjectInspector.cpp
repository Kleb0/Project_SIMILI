#include "UI/ObjectInspector.hpp"
#include "imgui.h"
#include <iostream>


ObjectInspector::ObjectInspector() {}

void ObjectInspector::setInspectedObject(ThreeDObject *object)
{

    if (object && !object->isInspectable()) {
        clearInspectedObject();
        return;
    }

    inspectedObject = object;
    isRenaming = false;

    if (object)
    {
        strncpy(nameEditBuffer, object->getName().c_str(), sizeof(nameEditBuffer));
        nameEditBuffer[sizeof(nameEditBuffer) - 1] = '\0';
    }
}

void ObjectInspector::setMultipleInspectedObjects(const std::list<ThreeDObject *> &objects)
{
    multipleInspectedObjects = objects;
    inspectedObject = nullptr;
}

void ObjectInspector::clearInspectedObject()
{
    inspectedObject = nullptr;
}

void ObjectInspector::clearMultipleInspectedObjects()
{
    multipleInspectedObjects.clear();
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
    bool isMultiple = !multipleInspectedObjects.empty();
    glm::vec3 pos(0.0f);

    if (isMultiple)
    {
        for (ThreeDObject *obj : multipleInspectedObjects)
            pos += obj->getPosition();
        pos /= (float)multipleInspectedObjects.size();
    }
    else
    {
        pos = inspectedObject->getPosition();
    }

    glm::vec3 newPos = pos;
    bool modified = false;

    ImGui::Text("Position:");
    ImGui::PushItemWidth(100.0f);

    modified |= ImGui::InputFloat("X", &pos.x, 0.1f, 1.0f);
    modified |= ImGui::InputFloat("Y", &pos.y, 0.1f, 1.0f);
    modified |= ImGui::InputFloat("Z", &pos.z, 0.1f, 1.0f);
    ImGui::PopItemWidth();

    if (modified)
    {
        glm::vec3 delta = newPos - pos;

        if (isMultiple)
        {
            for (ThreeDObject *obj : multipleInspectedObjects)
            {
                obj->setPosition(obj->getPosition() + delta);
            }
        }
        else
        {
            inspectedObject->setPosition(newPos);
        }
    }
}

void ObjectInspector::setRotation()
{
    bool isMultiple = !multipleInspectedObjects.empty();
    glm::vec3 rot(0.0f);

    if (isMultiple)
    {
        for (ThreeDObject *obj : multipleInspectedObjects)
            rot += obj->getRotation();
        rot /= (float)multipleInspectedObjects.size();
    }
    else
    {
        rot = inspectedObject->getRotation();
    }

    glm::vec3 newRot = rot;
    bool modified = false;

    ImGui::Text("Rotation:");
    ImGui::PushItemWidth(100.0f);

    modified |= ImGui::InputFloat("Pitch (X)", &rot.x, 1.0f, 5.0f);
    modified |= ImGui::InputFloat("Yaw (Y)", &rot.y, 1.0f, 5.0f);
    modified |= ImGui::InputFloat("Roll (Z)", &rot.z, 1.0f, 5.0f);

    ImGui::PopItemWidth();

    if (modified)
    {
        glm::vec3 delta = newRot - rot;

        if (isMultiple)
        {
            for (ThreeDObject *obj : multipleInspectedObjects)
            {
                obj->setRotation(obj->getRotation() + delta);
            }
        }
        else
        {
            inspectedObject->setRotation(newRot);
        }
    }
}

void ObjectInspector::setScale()
{
    bool isMultiple = !multipleInspectedObjects.empty();
    glm::vec3 scale(0.0f);

    if (isMultiple)
    {
        for (ThreeDObject *obj : multipleInspectedObjects)
            scale += obj->getScale();
        scale /= (float)multipleInspectedObjects.size();
    }
    else
    {
        scale = inspectedObject->getScale();
    }

    glm::vec3 newScale = scale;
    bool modified = false;

    ImGui::Text("Scale:");
    ImGui::PushItemWidth(100.0f);

    modified |= ImGui::InputFloat("X##scale", &scale.x, 0.1f, 0.5f);
    modified |= ImGui::InputFloat("Y##scale", &scale.y, 0.1f, 0.5f);
    modified |= ImGui::InputFloat("Z##scale", &scale.z, 0.1f, 0.5f);

    ImGui::PopItemWidth();
    if (modified)
    {
        if (isMultiple)
        {
            for (ThreeDObject *obj : multipleInspectedObjects)
            {
                obj->setScale(newScale);
            }
        }
        else
        {
            inspectedObject->setScale(newScale);
        }
    }
}

void ObjectInspector::dipslayGlobaleCoordinates(ThreeDObject* object)
{
    if (!object || !object->getParent())
        return;

    ImGui::Separator();
    ImGui::Text("Selected Object is a child | Displaying Pivot Point Coordinaters :");

    ThreeDObject* parent = object->getParent();
    glm::vec3 localOrigin = object->getOrigin();
    glm::vec3 worldOrigin = glm::vec3(object->getGlobalModelMatrix() * glm::vec4(localOrigin, 1.0f));

    ImGui::Text("Pivot Point Position: X: %.2f, Y: %.2f, Z: %.2f", worldOrigin.x, worldOrigin.y, worldOrigin.z);

    glm::vec3 parentWorldOrigin = glm::vec3(parent->getGlobalModelMatrix() * glm::vec4(parent->getOrigin(), 1.0f));
    ImGui::Text("Parent World Origin: X: %.2f, Y: %.2f, Z: %.2f", parentWorldOrigin.x, parentWorldOrigin.y, parentWorldOrigin.z);
}



void ObjectInspector::render()
{
    ImGui::Begin("Object Inspector");

    // case one : multiple objects selected
    if (!multipleInspectedObjects.empty())
    {
        ImGui::Text("Selected Objects (%d)", (int)multipleInspectedObjects.size());
        ImGui::Separator();

        int index = 1;
        int maxDisplay = 3;
        int count = 0;

        for (ThreeDObject *obj : multipleInspectedObjects)
        {
            if (!obj)
                continue;

            if (count < maxDisplay)
            {
                ImGui::BulletText("%s", obj->getName().c_str());
            }
            else if (count == maxDisplay)
            {
                ImGui::BulletText("...");
                break;
            }

            count++;
        }

        ImGui::Separator();
        setPosition();
        ImGui::Separator();
        setRotation();
        ImGui::Separator();
        setScale();
        ImGui::End();
        return;
    }

    // case two : single object selected
    if (inspectedObject)
    {
        renameObject();

        ImGui::Separator();
        setPosition();
        ImGui::Separator();
        setRotation();
        ImGui::Separator();
        setScale();
        ImGui::Separator();

        dipslayGlobaleCoordinates(inspectedObject);
    
    }
    else
    {
        ImGui::Text("No object selected");
    }

    ImGui::End();
}