#include "UI/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"
#include "UI/ThreeDWindow.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <Windows.h>

// void showErrorBox(const std::string &message, const std::string &title = "Error")
// {
//     MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
// }

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

void HierarchyInspector::multipleSelection(ThreeDObject *obj)
{

    auto it = std::find(multipleSelectedObjects.begin(), multipleSelectedObjects.end(), obj);
    if (it == multipleSelectedObjects.end())
    {
        multipleSelectedObjects.push_back(obj);
        std::cout << "[HierarchyInspector] Add object to multiple selection list: " << obj->getName() << std::endl;
    }

    else
    {
        multipleSelectedObjects.erase(it);
    }
    window->selectMultipleObjects(multipleSelectedObjects);
}

void HierarchyInspector::renameObject()
{
    if (!objectBeingRenamed)
        return;

    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::InputText("##edit", editingBuffer, sizeof(editingBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        objectBeingRenamed->setName(editingBuffer);
        objectBeingRenamed = nullptr;
    }
    if (!ImGui::IsItemActive() && !ImGui::IsItemHovered())
    {
        objectBeingRenamed = nullptr;
    }
}

void HierarchyInspector::notifyObjectDeleted(ThreeDObject *obj)
{
    if (!obj)
        return;

    if (selectedObjectInHierarchy == obj)
        selectedObjectInHierarchy = nullptr;

    if (objectBeingRenamed == obj)
        objectBeingRenamed = nullptr;
}

// ----------

void HierarchyInspector::render()
{

    ImGui::Begin("Hierarchy Inspector");
    ImGui::Text("List of objects in the current 3D scene :");

    bool clickedOnItem = false;

    auto drawList = [&](const std::list<ThreeDObject *> &list)
    {
        if (!selectedObjectInHierarchy && !multipleSelectedObjects.empty())
        {
            selectedObjectInHierarchy = multipleSelectedObjects.back();
        }

        for (ThreeDObject *obj : list)
        {
            if (!obj)
                continue;

            ImGui::PushID(obj);
            ImVec2 start = ImGui::GetCursorScreenPos();

            if (objectBeingRenamed == obj)
            {
                renameObject();
            }
            else
            {
                ImGui::Bullet();
                ImGui::SameLine();
                bool isSelected = (selectedObjectInHierarchy == obj);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
                ImGui::Selectable(obj->getName().c_str(), isSelected);
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    objectBeingRenamed = obj;
                    strncpy(editingBuffer, obj->getName().c_str(), sizeof(editingBuffer));
                    editingBuffer[sizeof(editingBuffer) - 1] = '\0';
                }

                // ------ Drag source for parenting ------
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    currentlyDraggedObject = obj;
                    ImGui::SetDragDropPayload("HIERARCHY_OBJECT", &currentlyDraggedObject, sizeof(ThreeDObject *));
                    ImGui::Text("%s", obj->getName().c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT"))
                    {
                        ThreeDObject *dragged = *(ThreeDObject **)payload->Data;
                        if (dragged && dragged != obj)
                        {
                            if (!dragged->isDescendantOf(obj))
                            {
                                SetParentByDragAndDrop(dragged, obj);
                                std::string message = "Objet \"" + obj->getName() + "\" est devenu le parent de l'objet \"" + dragged->getName() + "\".";
                                MessageBoxA(nullptr, message.c_str(), "Hiérarchie modifiée", MB_ICONINFORMATION | MB_OK);
                            }
                            else
                            {
                                MessageBoxA(nullptr, "Opération interdite : un objet ne peut pas devenir parent d'un de ses descendants.", "Erreur de hiérarchie", MB_ICONWARNING | MB_OK);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            ImVec2 end = ImGui::GetCursorScreenPos();
            float height = ImGui::GetTextLineHeightWithSpacing();
            float width = ImGui::GetContentRegionAvail().x;
            ImVec2 fullEnd = ImVec2(start.x + width, start.y + height);

            //------------
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
                (ImGui::IsMouseHoveringRect(start, fullEnd) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
            {
                clickedOnItem = true;

                if (ImGui::GetIO().KeyShift)
                {
                    // Add to multiple selection
                    if (std::find(multipleSelectedObjects.begin(), multipleSelectedObjects.end(), obj) == multipleSelectedObjects.end())
                    {
                        multipleSelectedObjects.push_back(obj);
                        obj->setSelected(true);
                    }

                    objectInspector->clearInspectedObject();
                    objectInspector->setMultipleInspectedObjects(multipleSelectedObjects);
                    window->setMultipleSelectedObjects(multipleSelectedObjects);
                }
                else
                {
                    // Single selection
                    for (auto *o : window->getObjects())
                        o->setSelected(false);

                    obj->setSelected(true);
                    multipleSelectedObjects.clear();
                    multipleSelectedObjects.push_back(obj);

                    objectInspector->clearMultipleInspectedObjects();
                    objectInspector->setInspectedObject(obj);
                    window->setMultipleSelectedObjects(multipleSelectedObjects);
                }

                selectedObjectInHierarchy = obj;

                // Notify the window to select this object
                if (!ImGui::GetIO().KeyShift)
                    window->externalSelect(obj);
            }

            //-----
            ImDrawList *draw_list = ImGui::GetWindowDrawList();

            if (obj->getSelected())
            {
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(100, 255, 100, 100));
            }
            else if (selectedObjectInHierarchy == obj)
            {
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 100, 100, 100));
            }
            else if (ImGui::IsMouseHoveringRect(start, fullEnd))
            {
                draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 255, 100, 60));
            }

            ImGui::PopID();
        }
    };

    std::list<ThreeDObject *> totalList = context ? context->getObjects() : std::list<ThreeDObject *>();

    if (window)
    {
        const std::vector<ThreeDObject *> &windowList = window->getObjects();
        for (ThreeDObject *obj : windowList)
        {
            if (obj && std::find(totalList.begin(), totalList.end(), obj) == totalList.end())
                totalList.push_back(obj);
        }
    }

    // ------- Draw the total list of objects avec indentation hiérarchique -------
    std::function<void(ThreeDObject *)> drawObjectRecursive;

    drawObjectRecursive = [&](ThreeDObject *obj)
    {
        if (!obj)
            return;

        ImGui::PushID(obj);
        ImVec2 start = ImGui::GetCursorScreenPos();

        bool isSelected = (selectedObjectInHierarchy == obj);
        bool hasChildren = !obj->getChildren().empty();

        bool nodeOpen = false;
        if (hasChildren)
        {
            nodeOpen = ImGui::TreeNodeEx(obj->getName().c_str(),
                                         ImGuiTreeNodeFlags_OpenOnArrow |
                                             (isSelected ? ImGuiTreeNodeFlags_Selected : 0));
        }
        else
        {
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
            ImGui::Selectable(obj->getName().c_str(), isSelected);
            ImGui::PopStyleColor(2);
        }

        // ----------- Double clic and rename ------------
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            objectBeingRenamed = obj;
            strncpy(editingBuffer, obj->getName().c_str(), sizeof(editingBuffer));
            editingBuffer[sizeof(editingBuffer) - 1] = '\0';
        }

        // -------------- Drag -------------
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            currentlyDraggedObject = obj;
            ImGui::SetDragDropPayload("HIERARCHY_OBJECT", &currentlyDraggedObject, sizeof(ThreeDObject *));
            ImGui::Text("%s", obj->getName().c_str());
            ImGui::EndDragDropSource();
        }

        // ---------------- Drop ---------------------
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("HIERARCHY_OBJECT"))
            {
                ThreeDObject *dragged = *(ThreeDObject **)payload->Data;
                if (dragged && dragged != obj)
                {
                    SetParentByDragAndDrop(dragged, obj);
                    std::string message = "Objet \"" + obj->getName() + "\" est devenu le parent de l'objet \"" + dragged->getName() + "\".";
                    MessageBoxA(nullptr, message.c_str(), "Hiérarchie modifiée", MB_ICONINFORMATION | MB_OK);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // ---------------- Selection ------------------------
        ImVec2 end = ImGui::GetCursorScreenPos();
        float height = ImGui::GetTextLineHeightWithSpacing();
        float width = ImGui::GetContentRegionAvail().x;
        ImVec2 fullEnd = ImVec2(start.x + width, start.y + height);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) ||
            (ImGui::IsMouseHoveringRect(start, fullEnd) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
        {
            clickedOnItem = true;

            if (ImGui::GetIO().KeyShift)
            {
                if (std::find(multipleSelectedObjects.begin(), multipleSelectedObjects.end(), obj) == multipleSelectedObjects.end())
                {
                    multipleSelectedObjects.push_back(obj);
                    obj->setSelected(true);
                }

                objectInspector->clearInspectedObject();
                objectInspector->setMultipleInspectedObjects(multipleSelectedObjects);
                window->setMultipleSelectedObjects(multipleSelectedObjects);
            }
            else
            {
                for (auto *o : window->getObjects())
                    o->setSelected(false);

                obj->setSelected(true);
                multipleSelectedObjects.clear();
                multipleSelectedObjects.push_back(obj);

                objectInspector->clearMultipleInspectedObjects();
                objectInspector->setInspectedObject(obj);
                window->setMultipleSelectedObjects(multipleSelectedObjects);
            }

            selectedObjectInHierarchy = obj;

            if (!ImGui::GetIO().KeyShift)
                window->externalSelect(obj);
        }

        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        if (obj->getSelected())
            draw_list->AddRectFilled(start, fullEnd, IM_COL32(100, 255, 100, 100));
        else if (selectedObjectInHierarchy == obj)
            draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 100, 100, 100));
        else if (ImGui::IsMouseHoveringRect(start, fullEnd))
            draw_list->AddRectFilled(start, fullEnd, IM_COL32(255, 255, 100, 60));

        // Recursive drawing of children
        if (hasChildren && nodeOpen)
        {
            for (ThreeDObject *child : obj->getChildren())
                drawObjectRecursive(child);

            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    // -------- Start of List's Drawing --------
    for (ThreeDObject *obj : totalList)
    {
        if (!obj || obj->getParent())
            continue;
        drawObjectRecursive(obj);
    }

    // // ------ End of List's Drawing ------

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !clickedOnItem)
    {
        if (objectInspector)
            objectInspector->clearInspectedObject();

        if (selectedObjectInHierarchy)
        {
            unselectObject(selectedObjectInHierarchy);
            multipleSelectedObjects.clear();
            objectInspector->clearMultipleInspectedObjects();
        }

        if (window)
            window->externalSelect(nullptr);
    }

    // Handle the drag of blog
    if (currentlyDraggedObject)
    {
        ImDrawList *draw_list = ImGui::GetForegroundDrawList();
        ImVec2 mouse_pos = ImGui::GetIO().MousePos;

        std::string nameStr = currentlyDraggedObject->getName();
        const char *name = nameStr.c_str();

        ImVec2 text_size = ImGui::CalcTextSize(name);
        ImVec2 padding = ImVec2(6, 4);
        ImVec2 bg_min = mouse_pos;
        ImVec2 bg_max = ImVec2(mouse_pos.x + text_size.x + padding.x * 2, mouse_pos.y + text_size.y + padding.y * 2);

        draw_list->AddRectFilled(bg_min, bg_max, IM_COL32(50, 50, 50, 200), 5.0f);
        draw_list->AddRect(bg_min, bg_max, IM_COL32(200, 200, 200, 255), 5.0f);
        draw_list->AddText(ImVec2(mouse_pos.x + padding.x, mouse_pos.y + padding.y), IM_COL32(255, 255, 255, 255), name);

        // If the mouse is released, we stop dragging and currentlyDraggedObject is set to nullptr
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            currentlyDraggedObject = nullptr;
        }
    }

    ImGui::End();
}

// ------------

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
    const std::list<ThreeDObject *> emptyList;
    const std::list<ThreeDObject *> &contextList = context ? context->getObjects() : emptyList;
    const auto &windowList = window->getObjects();

    auto it = std::find(contextList.begin(), contextList.end(), selected);
    if (it != contextList.end())
    {
        selectedObjectInHierarchy = *it;
        return;
    }

    auto it2 = std::find(windowList.begin(), windowList.end(), selected);
    if (it2 != windowList.end())
    {
        selectedObjectInHierarchy = *it2;
    }
}

void HierarchyInspector::unselectObject(ThreeDObject *obj)
{
    if (!obj)
        return;

    if (selectedObjectInHierarchy == obj)
    {
        selectedObjectInHierarchy = nullptr;
        // std::cout << "[HierarchyInspector] Unselected object: " << obj->getName() << std::endl;
    }
}

void HierarchyInspector::clearMultipleSelection()
{
    multipleSelectedObjects.clear();
}

void HierarchyInspector::SetParentByDragAndDrop(ThreeDObject *child, ThreeDObject *newParent)
{
    if (!child || !newParent || child == newParent)
        return;

    if (child->isDescendantOf(newParent))
        return;

    child->setParent(newParent);
}

ThreeDObject *HierarchyInspector::getSelectedObject() const
{
    return selectedObjectInHierarchy;
}
