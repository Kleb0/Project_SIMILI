#include "UI/HierarchyInspectorLogic/HierarchyListDrawing.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "WorldObjects/EmptyDummy.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>

HierarchyListDrawing::HierarchyListDrawing(HierarchyInspector* inspector)
    : inspector(inspector) {}


void HierarchyListDrawing::drawSlotsList(bool& clickedOnItem)
{
    const int totalSlots = 50;

    // --------- STEP 1 Initialize only once --------- //
    if (!inspector->slotsListInitialized)
    {
        inspector->emptySlotPlaceholders.clear();
        inspector->mergedHierarchyList.clear();

        for (int i = 0; i < totalSlots; ++i)
        {
            auto dummy = std::make_unique<EmptyDummy>();
            dummy->setSlot(i);
            dummy->setName("Empty Slot");
            EmptyDummy* rawPtr = dummy.get();
            inspector->emptySlotPlaceholders.push_back(std::move(dummy));
            inspector->mergedHierarchyList.push_back(static_cast<ThreeDObject*>(rawPtr));
        }

        inspector->slotsListInitialized = true;
    }

    // --------- Drop operation (drag-and-drop release) --------- //
    bool isDragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && inspector->currentlyDraggedObject != nullptr;

    if (!isDragging && inspector->currentlyDraggedObject != nullptr)
    {
        std::cout << "[HierarchyInspector] Dropping object: " << inspector->currentlyDraggedObject->getName() << std::endl;

        if (inspector->childToDropOn)
        {
            inspector->dropOnObject(inspector->childToDropOn, inspector->currentlyDraggedObject, -1);
        }
        else if (inspector->dropToSlotIndex != -1)
        {
            inspector->dropOnSlot(inspector->currentlyDraggedObject, inspector->dropToSlotIndex);
        }

        // Reset state
        inspector->currentlyDraggedObject = nullptr;
        inspector->dropToSlotIndex = -1;
    }
    // -------- End of drop operation --------- //

    // --------- STEP 2 Assign ThreeDObjects to slots --------- //
    if (inspector->context && !inspector->objectsAssignedOnce)
    {
        const std::list<ThreeDObject*>& sceneObjects = inspector->context->getObjects();

        int nextFreeSlot = 0;

        for (ThreeDObject* obj : sceneObjects)
        {
            if (!obj)
                continue;

            if (obj->isParented || obj->IsVertice())
                continue; 

            int slot = obj->getSlot();

            if (slot < 0 || slot >= totalSlots)
            {
                while (nextFreeSlot < totalSlots && dynamic_cast<EmptyDummy*>(inspector->mergedHierarchyList[nextFreeSlot]) == nullptr)
                    ++nextFreeSlot;

                if (nextFreeSlot >= totalSlots)
                {
                    std::cerr << "Warning: no free slot available for object '" << obj->getName() << "'" << std::endl;
                    continue;
                }

                obj->setSlot(nextFreeSlot);
                slot = nextFreeSlot;
                ++nextFreeSlot;
            }

            inspector->mergedHierarchyList[slot] = obj;
        }

        inspector->objectsAssignedOnce = true;
    }


    // --------- STEP 3 Draw all slots --------- //
    for (int i = 0; i < totalSlots; ++i)
    {
        ThreeDObject* obj = inspector->mergedHierarchyList[i];
        float slotHeight = 26.0f;
        ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, slotHeight);

        bool isEven = (i % 2 == 0);
        ImVec4 defaultBgColor = isEven ? ImVec4(0.8f, 0.8f, 0.8f, 1.0f)
                                    : ImVec4(0.1f, 0.2f, 0.5f, 1.0f);
        ImVec4 selectedBgColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
        bool isSlotSelected = obj && obj->getSelected();

        ImVec4 textColor = isSlotSelected
                        ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
                        : (isEven ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
                                    : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        ImGui::PushStyleColor(ImGuiCol_Button, isSlotSelected ? selectedBgColor : defaultBgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        std::string label;
        if (obj && obj->getName() != "Empty Slot")
        {
            std::string toggleIcon = obj->getChildren().empty() ? "  " : (obj->expanded ? u8"\u25BC " : u8"\u25B6 ");
            label = toggleIcon + "--- [" + obj->getName() + "] ---";
        }
        else
        {
            label = "--- Empty Slot [" + std::to_string(i) + "] ---";
        }

        ImGui::PushFont(inspector->unicodeFont);
        ImGui::Button(label.c_str(), size);
        ImGui::PopFont();

        // prevent from dragging empty slots
        if (obj && obj->getName() != "Empty Slot")
        {
            // - Toggle open/close
            if (ImGui::IsItemHovered() &&
                ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
            {
                obj->expanded = !obj->expanded;
            }

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
                inspector->currentlyDraggedObject = obj;

            if (inspector->currentlyDraggedObject)
                inspector->dragObject(inspector->currentlyDraggedObject, i);

            inspector->clickOnSlot(clickedOnItem, i, obj);

            if (obj->expanded)
                drawChildSlots(obj, clickedOnItem);
        }
        else if (obj) 
        {
            if (inspector->currentlyDraggedObject)
                inspector->dragObject(inspector->currentlyDraggedObject, i);

            inspector->clickOnSlot(clickedOnItem, i, obj);
        }

        ImGui::PopStyleColor(2);
    }
    // ----- end of slots drawing ----- //
}

void HierarchyListDrawing::drawChildSlots(ThreeDObject* parent,  bool& clickedOnItem)
{
    if (!parent || parent->getChildren().empty())
        return;

    ImGui::Indent(20.0f); 

    for (ThreeDObject* child : parent->getChildren())
    {
        if (!child || child->IsVertice()) continue;
        
        // ---------- Begin Child Slot Rendering ---------- //

        float slotHeight = 26.0f;
        ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, slotHeight);

        bool isSelected = child->getSelected();
        ImVec4 bgColor = isSelected ? ImVec4(0.2f, 0.8f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImVec4 textColor = isSelected ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        // if the child has a children too, normally we should see a toggle icon and being able to expand/collapse it
        std::string toggleIcon = child->getChildren().empty() ? "  " : (child->expanded ? u8"\u25BC " : u8"\u25B6 ");
        std::string label = toggleIcon + child->getName();

        ImGui::PushFont(inspector->unicodeFont);
        ImGui::Button(label.c_str(), size);
        ImGui::PopFont();

        // ------ After Child Button rendering ------ //

        // ---------- Here we detect if we are above a child when an object is being dragged ---------- //
        if (inspector->currentlyDraggedObject && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
        {

            ImVec2 rectMin = ImGui::GetItemRectMin();
            ImVec2 rectMax = ImGui::GetItemRectMax();
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            // Draw a red rectangle around the hovered child slot
            draw_list->AddRect(rectMin, rectMax, IM_COL32(255, 0, 0, 255), 0.0f, 0, 3.0f);

            inspector->childToDropOn = child;
            // - set the child as the parent of currently dragged object
            inspector->currentlyDraggedObject->setParent(child);
        }

        // ----------------- End of child detection ----------------- //


        if (ImGui::IsItemHovered() &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
        {
            child->expanded = !child->expanded;
        }

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
        {
            inspector->currentlyDraggedObject = child;
        }

        inspector->clickOnSlot(clickedOnItem, -1, child);

        ImGui::PopStyleColor(2);

        if (child->expanded)
            drawChildSlots(child, clickedOnItem);
        // ---------- End of Child Slot Rendering ---------- //
    }

    ImGui::Unindent(20.0f);

}
