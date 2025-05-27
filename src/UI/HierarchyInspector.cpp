#include "UI/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"
#include "UI/ThreeDWindow.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>

HierarchyInspector::HierarchyInspector() {}

void HierarchyInspector::setObjectInspector(ObjectInspector *inspector)
{
    objectInspector = inspector;
}

void HierarchyInspector::setContext(OpenGLContext *ctxt)
{
    context = ctxt;
}

void HierarchyInspector::setThreeDWindow(ThreeDWindow *win)
{
    window = win;
}

void HierarchyInspector::selectObject(ThreeDObject *obj)
{
    selectedObjectInHierarchy = obj;

    if (objectInspector)
        objectInspector->setInspectedObject(obj);

    if (window)
        window->externalSelect(obj);
}

void HierarchyInspector::selectInList(ThreeDObject *obj)
{
    selectedObjectInHierarchy = obj;
}

void HierarchyInspector::render()
{

    ImGui::Begin("Hierarchy Inspector");
    ImGui::Text("List of objects in the current 3D scene :");

    bool clickedOnItem = false;

    auto drawList = [&](const std::vector<ThreeDObject *> &list)
    {
        for (ThreeDObject *obj : list)
        {
            if (!obj)
                continue;

            ImGui::PushID(obj);
            ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::BulletText("%s", obj->getName().c_str());
            ImVec2 end = ImGui::GetCursorScreenPos();
            float height = ImGui::GetTextLineHeightWithSpacing();
            float width = ImGui::GetContentRegionAvail().x;
            ImVec2 fullEnd = ImVec2(start.x + width, start.y + height);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                (ImGui::IsMouseHoveringRect(start, fullEnd) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
            {
                selectedObjectInHierarchy = obj;
                clickedOnItem = true;

                if (objectInspector)
                    objectInspector->setInspectedObject(obj);

                if (window)
                    window->externalSelect(obj);
            }

            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            if (selectedObjectInHierarchy == obj)
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 100, 100, 100));
            else if (ImGui::IsMouseHoveringRect(start, fullEnd))
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 255, 100, 60));

            ImGui::PopID();
        }
    };

    if (context)
        drawList(context->getObjects());

    if (window)
    {
        const std::vector<ThreeDObject *> &windowList = window->getObjects();
        const std::vector<ThreeDObject *> &contextList = context ? context->getObjects() : std::vector<ThreeDObject *>();

        for (ThreeDObject *obj : windowList)
        {
            if (!obj)
                continue;

            if (std::find(contextList.begin(), contextList.end(), obj) != contextList.end())
                continue;

            ImGui::PushID(obj);
            ImVec2 start = ImGui::GetCursorScreenPos();
            ImGui::BulletText("%s", obj->getName().c_str());
            ImVec2 end = ImGui::GetCursorScreenPos();
            float height = ImGui::GetTextLineHeightWithSpacing();
            float width = ImGui::GetContentRegionAvail().x;
            ImVec2 fullEnd = ImVec2(start.x + width, start.y + height);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                (ImGui::IsMouseHoveringRect(start, fullEnd) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
            {
                selectedObjectInHierarchy = obj;
                clickedOnItem = true;

                if (window)
                    window->externalSelect(obj);
            }

            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            if (selectedObjectInHierarchy == obj)
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 100, 100, 100));
            else if (ImGui::IsMouseHoveringRect(start, fullEnd))
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 255, 100, 60));

            ImGui::PopID();
        }
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !clickedOnItem)
    {
        if (objectInspector)
            objectInspector->clearInspectedObject();

        if (selectedObjectInHierarchy)
            unselectobject(selectedObjectInHierarchy);

        if (window)
            window->externalSelect(nullptr);
    }

    ImGui::End();
}

void HierarchyInspector::selectFromThreeDWindow()
{

    // std::cout << "[HierarchyInspector] call from ThreeD Window" << std::endl;
    if (!window)
        return;

    ThreeDObject *selected = nullptr;

    if (window)
        selected = window->getSelectedObject();

    if (!selected)
    {
        selectedObjectInHierarchy = nullptr;
        return;
    }

    const auto &contextList = context ? context->getObjects() : std::vector<ThreeDObject *>();
    const auto &windowList = window->getObjects();

    auto it = std::find(contextList.begin(), contextList.end(), selected);
    if (it != contextList.end())
    {
        selectedObjectInHierarchy = *it;
        // std::cout << "[HierarchyInspector] TEST A - Selected object from 3D window: " << selected->getName() << std::endl;
        return;
    }

    it = std::find(windowList.begin(), windowList.end(), selected);
    if (it != windowList.end())
    {
        selectedObjectInHierarchy = *it;
        // std::cout << "[HierarchyInspector]  TEST B - Selected object from 3D window: " << selected->getName() << std::endl;
    }
}

void HierarchyInspector::unselectobject(ThreeDObject *obj)
{
    if (!obj)
        return;

    if (selectedObjectInHierarchy == obj)
    {
        selectedObjectInHierarchy = nullptr;
        // std::cout << "[HierarchyInspector] Unselected object: " << obj->getName() << std::endl;
    }
}

ThreeDObject *HierarchyInspector::getSelectedObject() const
{
    return selectedObjectInHierarchy;
}