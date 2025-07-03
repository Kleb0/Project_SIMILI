#define IMGUI_ENABLE_ASSERTS
#include "UI/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"
#include "UI/ThreeDWindow.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <typeinfo>
#define NOMINMAX
#include <Windows.h>
#include <filesystem> 
#include "WorldObjects/ThreeDObject.hpp"

void showErrorBox(const std::string &message, const std::string &title = "Error")
{
    MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
}

HierarchyInspector::HierarchyInspector()
{
    // for (int i = 0; i < 50; i++)
    // {
    //     SlotsList.push_back(nullptr);
    // }

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 3;
    font_cfg.PixelSnapH = false;

    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF,
        0x2190, 0x21FF,
        0x25A0, 0x25FF, 
        0,
    };

    std::filesystem::path exePath = std::filesystem::current_path();
    std::filesystem::path projectRoot = exePath.parent_path().parent_path(); 

    std::filesystem::path fontPath = projectRoot / "assets/fonts/DejaVuSans.ttf";
    unicodeFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f, &font_cfg, glyph_ranges);

    // if (!unicodeFont)
    // {
    //     showErrorBox("Font not loaded correctly! Path: " + fontPath.string(), "Font Error");
    // }
        io.Fonts->Build();
}


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

    if (obj && !obj->isInspectable()) {
        if (objectInspector)
            objectInspector->clearInspectedObject();
        return;
    }

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

// ---------- List rendering ----------

void HierarchyInspector::render()
{
    ImGui::Begin("Hierarchy Inspector");
    ImGui::Text("List of objects in the current 3D scene :");

    bool clickedOnItem = false;
    slotsList(clickedOnItem);

    // Clear selection if clicking on empty space
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

    // Drag and drop ghost object
    ImGui::End();
}

void HierarchyInspector::slotsList(bool& clickedOnItem)
{
    const int totalSlots = 50;

    // --------- Initialize only once --------- //
    if (!slotsListInitialized)
    {
        emptySlotPlaceholders.clear();
        mergedHierarchyList.clear();

        for (int i = 0; i < totalSlots; ++i)
        {
            auto dummy = std::make_unique<EmptyDummy>();
            dummy->setSlot(i);
            dummy->setName("Empty Slot");
            EmptyDummy* rawPtr = dummy.get();
            emptySlotPlaceholders.push_back(std::move(dummy));
            mergedHierarchyList.push_back(static_cast<ThreeDObject*>(rawPtr));
        }

        slotsListInitialized = true;
    }

    // --------- Drop operation (drag-and-drop release) --------- //
    bool isDragging = ImGui::IsMouseDown(ImGuiMouseButton_Left) && currentlyDraggedObject != nullptr;
    if (!isDragging && currentlyDraggedObject != nullptr && dropToSlotIndex != -1)
    {
        dropOnSlot(currentlyDraggedObject, dropToSlotIndex);
        currentlyDraggedObject = nullptr;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        currentlyDraggedObject = nullptr;

    // --------- Assign ThreeDObjects to slots --------- //
    if (context && !objectsAssignedOnce)
    {
        const std::list<ThreeDObject*>& sceneObjects = context->getObjects();

        int nextFreeSlot = 0;

        for (ThreeDObject* obj : sceneObjects)
        {
            if (!obj)
                continue;

            int slot = obj->getSlot();

            if (slot < 0 || slot >= totalSlots)
            {
                while (nextFreeSlot < totalSlots && dynamic_cast<EmptyDummy*>(mergedHierarchyList[nextFreeSlot]) == nullptr)
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

            mergedHierarchyList[slot] = obj;

            std::cout << "Inserted object '" << obj->getName() << "' at slot " << slot << std::endl;
        }

        objectsAssignedOnce = true;
    }


    // --------- Draw all slots --------- //
    for (int i = 0; i < totalSlots; ++i)
    {
        float slotHeight = 26.0f;
        ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, slotHeight);

        bool isEven = (i % 2 == 0);
        ImVec4 defaultBgColor = isEven ? ImVec4(0.8f, 0.8f, 0.8f, 1.0f)
                                       : ImVec4(0.1f, 0.2f, 0.5f, 1.0f);
        ImVec4 selectedBgColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);

        ThreeDObject* obj = mergedHierarchyList[i];
        bool isSlotSelected = obj && obj->getSelected();

        ImVec4 textColor = isSlotSelected
                           ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
                           : (isEven ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f)
                                     : ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        ImGui::PushStyleColor(ImGuiCol_Button, isSlotSelected ? selectedBgColor : defaultBgColor);
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);

        std::string label = " --- " + obj->getName() + " [" + std::to_string(i) + "] --- ";
        ImGui::Button(label.c_str(), size);

        if (obj && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f))
            currentlyDraggedObject = obj;

        if (obj && currentlyDraggedObject != nullptr)
            dragObject(currentlyDraggedObject, i);

        if (obj)
            clickOnSlot(clickedOnItem, i, obj);

        ImGui::PopStyleColor(2);
    }
}


void HierarchyInspector::clickOnSlot(bool& clickedOnItem, int index, ThreeDObject* obj)
{

    if (!ImGui::IsItemClicked(ImGuiMouseButton_Left))
        return;

    clickedOnItem = true;

    if (!obj || !obj->isInspectable())
    {
        for (auto* o : window->getObjects())
            o->setSelected(false);

        multipleSelectedObjects.clear();
        objectInspector->clearMultipleInspectedObjects();
        objectInspector->clearInspectedObject();

        selectedObjectInHierarchy = nullptr;
        window->externalSelect(nullptr);    

        std::cout << "[HierarchyInspector] Clicked on slot " << index
            << " containing: " << (obj ? typeid(*obj).name() : "nullptr")
            << " (not inspectable)" << std::endl;

        return;
    }

    std::cout << "[HierarchyInspector] Clicked on slot containing an item : " << index
            << " (type of content: " << typeid(*obj).name() << ")" << std::endl;

     if (ImGui::GetIO().KeyShift && lastSelectedSlotIndex != -1)
    {
        int start = std::min(index, lastSelectedSlotIndex);
        int end = std::max(index, lastSelectedSlotIndex);

        for (int i = start; i <= end; ++i)
        {
            if (i >= 0 && i < static_cast<int>(mergedHierarchyList.size()))
            {
                ThreeDObject* rangeObj = static_cast<ThreeDObject*>(mergedHierarchyList[i]);

                if (rangeObj && std::find(multipleSelectedObjects.begin(), multipleSelectedObjects.end(), rangeObj) == multipleSelectedObjects.end())
                {
                    multipleSelectedObjects.push_back(rangeObj);
                    rangeObj->setSelected(true);
                }
            }
        }

        objectInspector->clearInspectedObject();
        objectInspector->setMultipleInspectedObjects(multipleSelectedObjects);
        window->setMultipleSelectedObjects(multipleSelectedObjects);
        selectedObjectInHierarchy = obj;
    }
    else
    {
        for (auto* o : window->getObjects())
            o->setSelected(false);

        obj->setSelected(true);
        multipleSelectedObjects.clear();
        multipleSelectedObjects.push_back(obj);

        objectInspector->clearMultipleInspectedObjects();
        objectInspector->setInspectedObject(obj);
        window->setMultipleSelectedObjects(multipleSelectedObjects);

        lastSelectedSlotIndex = index;
    }

    selectedObjectInHierarchy = obj;

    if (!ImGui::GetIO().KeyShift)
        window->externalSelect(obj);

}



void HierarchyInspector::dragObject(ThreeDObject* draggedObj, int index)
{

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        lastSelectedObject = draggedObj;
        if(currentlyDraggedObject)
        {
             if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
            {
                // std::cout << "[HierarchyInspector] Mouse over slot: " << index << std::endl;

                ImVec2 rectMin = ImGui::GetItemRectMin();
                ImVec2 rectMax = ImGui::GetItemRectMax();
                ImDrawList* draw_list = ImGui::GetForegroundDrawList();
                draw_list->AddRect(rectMin, rectMax, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.5f);
                mouseOveringSlotIndex = index;
            }       
        }

        
        std::string ghostLabel = currentlyDraggedObject->getName();

        if (mouseOveringSlotIndex != -1)
        {
            ghostLabel += " → Slot [" + std::to_string(mouseOveringSlotIndex) + "]";
            dropToSlotIndex = mouseOveringSlotIndex;
        }

        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 textSize = ImGui::CalcTextSize(ghostLabel.c_str());
        ImVec2 padding = ImVec2(10.0f, 6.0f);
        ImVec2 bgMin = ImVec2(mousePos.x + 16.0f, mousePos.y + 16.0f);
        ImVec2 bgMax = ImVec2(bgMin.x + textSize.x + padding.x * 2, bgMin.y + textSize.y + padding.y * 2);

        ImDrawList* draw_list = ImGui::GetForegroundDrawList();
        draw_list->AddRectFilled(bgMin, bgMax, IM_COL32(150, 150, 150, 200), 4.0f);
        draw_list->AddText(ImVec2(bgMin.x + padding.x, bgMin.y + padding.y), IM_COL32(0, 0, 0, 255), ghostLabel.c_str());
    }

  

    if (index < 0 || index >= static_cast<int>(mergedHierarchyList.size()))
        return;
   
}

void HierarchyInspector::dropOnSlot(ThreeDObject* obj, int index)
{
    std::cout << "[HierarchyInspector] dropOnSlot called with index: " << index << std::endl;

    void* ptr = mergedHierarchyList[index];
    ThreeDObject* base = static_cast<ThreeDObject*>(ptr);
    std::cout << "RTTI typeid: " << typeid(*base).name() << std::endl;
    EmptyDummy* dummy = dynamic_cast<EmptyDummy*>(base);


    if (dummy && dummy->isDummy())
    {
        std::cout << "[dragOnEmptySlot] Slot index (mergedHierarchyList): " << index << std::endl;
        std::cout << "[dragOnEmptySlot] Dummy slot index (getSlot): " << dummy->getSlot() << std::endl;
        std::cout << "[dragOnEmptySlot] Dragged object name: " << obj->getName() << std::endl;
        std::cout << "[dragOnEmptySlot] Dragged object slot: " << obj->getSlot() << std::endl;
    }

    {
        std::cout << "[dragOnEmptySlot] dragged obect with name: " << obj->getName() << " and type: "
                  << typeid(*obj).name() << " dropped on slot index: " << index << std::endl;
        std::cout << "dropped slot index contains an object with name: " << base->getName() << " with type: " 
        << typeid(*base).name() <<  " at index: " << index << std::endl;

    }
    dropToSlotIndex = -1;
    
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