#include "UI/HierarchyInspectorLogic/HandleHierarchyInteractions.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"
#include "UI/ThreeDWindow.hpp"
#include "Engine/SimiliSelector.hpp"
#include <imgui.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>
#include <algorithm>

HandleHierarchyInteractions::HandleHierarchyInteractions(HierarchyInspector* inspector)
    : inspector(inspector) {}

void HandleHierarchyInteractions::clickOnSlot(bool& clickedOnItem, int index, ThreeDObject* obj)
{
    if (!ImGui::IsItemClicked(ImGuiMouseButton_Left)) return;
    clickedOnItem = true;

    auto& selector = inspector->window->getSimiliSelector();
    auto& multiSel = inspector->multipleSelectedObjects;

    if (!obj || !obj->isInspectable()) {
        selector.clearSelection();
        multiSel.clear();

        if (inspector->objectInspector) {
            inspector->objectInspector->clearMultipleInspectedObjects();
            inspector->objectInspector->clearInspectedObject();
        }

        inspector->selectedObjectInHierarchy = nullptr;
        selector.externalSelect(obj);
        return;
    }

    if (ImGui::GetIO().KeyShift && inspector->lastSelectedSlotIndex != -1) {
        int start = std::min(index, inspector->lastSelectedSlotIndex);
        int end = std::max(index, inspector->lastSelectedSlotIndex);

        for (int i = start; i <= end; ++i) {
            if (i >= 0 && i < static_cast<int>(inspector->mergedHierarchyList.size())) {
                ThreeDObject* rangeObj = inspector->mergedHierarchyList[i];
                if (rangeObj && std::find(multiSel.begin(), multiSel.end(), rangeObj) == multiSel.end()) {
                    multiSel.push_back(rangeObj);
                    rangeObj->setSelected(true);
                }
            }
        }

        if (inspector->objectInspector) {
            inspector->objectInspector->clearInspectedObject();
            inspector->objectInspector->setMultipleInspectedObjects(multiSel);
        }

        selector.setMultipleSelectedObjects(multiSel);
        inspector->selectedObjectInHierarchy = obj;
    } else {
        selector.clearSelection();
        obj->setSelected(true);
        multiSel.clear();
        multiSel.push_back(obj);

        if (inspector->objectInspector) {
            inspector->objectInspector->clearMultipleInspectedObjects();
            inspector->objectInspector->setInspectedObject(obj);
        }

        selector.setMultipleSelectedObjects(multiSel);
        inspector->lastSelectedSlotIndex = index;
    }

    inspector->selectedObjectInHierarchy = obj;

    if (!ImGui::GetIO().KeyShift)
        selector.externalSelect(obj);
}

void HandleHierarchyInteractions::dragObject(ThreeDObject* draggedObj, int index)
{
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;

    inspector->lastSelectedObject = draggedObj;

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
        inspector->mouseOveringSlotIndex = index;
        inspector->childToDropOn = nullptr;

        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 rectMax = ImGui::GetItemRectMax();
        ImGui::GetForegroundDrawList()->AddRect(rectMin, rectMax, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.5f);
    }

    std::string ghostLabel = inspector->currentlyDraggedObject->getName();

    if (inspector->childToDropOn) {
        ghostLabel += " → " + inspector->childToDropOn->getName();
        inspector->dropToSlotIndex = -1;
    } else if (inspector->mouseOveringSlotIndex != -1) {
        ghostLabel += " → Slot [" + std::to_string(inspector->mouseOveringSlotIndex) + "]";
        inspector->dropToSlotIndex = inspector->mouseOveringSlotIndex;
    } else {
        inspector->dropToSlotIndex = -1;
    }

    ImVec2 mousePos = ImGui::GetMousePos();
    ImVec2 textSize = ImGui::CalcTextSize(ghostLabel.c_str());
    ImVec2 padding = ImVec2(10.0f, 6.0f);
    ImVec2 bgMin = ImVec2(mousePos.x + 16.0f, mousePos.y + 16.0f);
    ImVec2 bgMax = ImVec2(bgMin.x + textSize.x + padding.x * 2, bgMin.y + textSize.y + padding.y * 2);

    auto* draw_list = ImGui::GetForegroundDrawList();
    draw_list->AddRectFilled(bgMin, bgMax, IM_COL32(150, 150, 150, 200), 4.0f);
    draw_list->AddText(ImVec2(bgMin.x + padding.x, bgMin.y + padding.y), IM_COL32(0, 0, 0, 255), ghostLabel.c_str());
}

void HandleHierarchyInteractions::dropOnSlot(ThreeDObject* obj, int index)
{
    ThreeDObject* base = inspector->mergedHierarchyList[index];
    auto* dummy = dynamic_cast<EmptyDummy*>(base);

    if (dummy && dummy->isDummy()) {
        glm::mat4 global = obj->getGlobalModelMatrix();
        if (auto* parent = obj->getParent()) {
            parent->removeChild(obj);
            obj->removeParent();
            obj->isParented = false;
        }
        obj->setModelMatrix(global);
        obj->setOrigin(inspector->context->worldCenter);
        inspector->exchangeSlots(obj, index);
    } else if (base) {
        dropOnObject(base, obj, index);
        inspector->dropToSlotIndex = -1;
    }
}

void HandleHierarchyInteractions::dropOnObject(ThreeDObject* parent, ThreeDObject* child, int index)
{
    if (child->getParent() == parent) return;

    glm::vec3 parentOrigin = glm::vec3(parent->getGlobalModelMatrix() * glm::vec4(parent->getOrigin(), 1.0f));
    glm::mat4 childBefore = child->getGlobalModelMatrix();

    if (auto* oldParent = child->getParent()) {
        oldParent->removeChild(child);
        child->removeParent();
        child->isParented = false;
    }

    parent->addChild(child);
    parent->expanded = true;
    child->setParent(parent);
    child->isParented = true;

    glm::mat4 childAfter = child->getGlobalModelMatrix();
    glm::vec3 newLocalOrigin = glm::vec3(glm::inverse(childAfter) * glm::vec4(parentOrigin, 1.0f));
    child->setOrigin(newLocalOrigin);

    inspector->redrawSlotsList();
}

void HandleHierarchyInteractions::selectObject(ThreeDObject* obj)
{
    inspector->selectedObjectInHierarchy = obj;
    if (!obj || !obj->isInspectable()) {
        if (inspector->objectInspector)
            inspector->objectInspector->clearInspectedObject();
        return;
    }

    if (inspector->objectInspector)
        inspector->objectInspector->setInspectedObject(obj);

    if (inspector->window)
        inspector->window->getSimiliSelector().externalSelect(obj);
}

void HandleHierarchyInteractions::selectInList(ThreeDObject* obj)
{
    inspector->selectedObjectInHierarchy = obj;
}

void HandleHierarchyInteractions::multipleSelection(ThreeDObject* obj)
{
    auto& list = inspector->multipleSelectedObjects;
    auto it = std::find(list.begin(), list.end(), obj);
    if (it == list.end()) {
        list.push_back(obj);
        std::cout << "[HierarchyInspector] Add object to multiple selection list: " << obj->getName() << std::endl;
    } else {
        list.erase(it);
    }

    inspector->window->getSimiliSelector().setMultipleSelectedObjects(list);
}
