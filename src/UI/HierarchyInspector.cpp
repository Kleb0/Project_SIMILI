#define IMGUI_ENABLE_ASSERTS
#include "UI/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"
#include "UI/ThreeDWindow.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <typeinfo>
#include <filesystem> 
#include "WorldObjects/ThreeDObject.hpp"

HierarchyInspector::HierarchyInspector()
{

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 3;
    font_cfg.PixelSnapH = false;

    static const ImWchar glyph_ranges[] = {
        0x0020, 0x00FF,    
        0x2190, 0x21FF,    
        0x25A0, 0x25FF,     
        0x25B0, 0x25BF,     
        0,
    };

    std::filesystem::path exePath = std::filesystem::current_path();
    std::filesystem::path projectRoot = exePath.parent_path().parent_path(); 

    std::filesystem::path fontPath = projectRoot / "assets/fonts/DejaVuSans.ttf";
    io.Fonts->Clear();
    unicodeFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f, &font_cfg, glyph_ranges);
    io.Fonts->Build();
}


// -------- Initalisition and management --------- //
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
    drawSlotsList(clickedOnItem);

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

void HierarchyInspector::drawSlotsList(bool& clickedOnItem)
{
    const int totalSlots = 50;

    // --------- STEP 1 Initialize only once --------- //
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

    if (!isDragging && currentlyDraggedObject != nullptr)
    {
        std::cout << "[HierarchyInspector] Dropping object: " << currentlyDraggedObject->getName() << std::endl;

        if (childToDropOn)
        {
            dropOnObject(childToDropOn, currentlyDraggedObject, -1);
        }
        else if (dropToSlotIndex != -1)
        {
            dropOnSlot(currentlyDraggedObject, dropToSlotIndex);
        }

        // Reset state
        currentlyDraggedObject = nullptr;
        dropToSlotIndex = -1;
    }
    // -------- End of drop operation --------- //

    // --------- STEP 2 Assign ThreeDObjects to slots --------- //
    if (context && !objectsAssignedOnce)
    {
        const std::list<ThreeDObject*>& sceneObjects = context->getObjects();

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
        }

        objectsAssignedOnce = true;
    }


    // --------- STEP 3 Draw all slots --------- //
    for (int i = 0; i < totalSlots; ++i)
    {
        ThreeDObject* obj = mergedHierarchyList[i];
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

        ImGui::PushFont(unicodeFont);
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
                currentlyDraggedObject = obj;

            if (currentlyDraggedObject)
                dragObject(currentlyDraggedObject, i);

            clickOnSlot(clickedOnItem, i, obj);

            if (obj->expanded)
                drawChildSlots(obj, clickedOnItem);
        }
        else if (obj) 
        {
            if (currentlyDraggedObject)
                dragObject(currentlyDraggedObject, i);

            clickOnSlot(clickedOnItem, i, obj);
        }

        ImGui::PopStyleColor(2);
    }
    // ----- end of slots drawing ----- //
}

void HierarchyInspector::drawChildSlots(ThreeDObject* parent,  bool& clickedOnItem)
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

        ImGui::PushFont(unicodeFont);
        ImGui::Button(label.c_str(), size);
        ImGui::PopFont();

        // ------ After Child Button rendering ------ //

        // ---------- Here we detect if we are above a child when an object is being dragged ---------- //
        if (currentlyDraggedObject && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
        {

            ImVec2 rectMin = ImGui::GetItemRectMin();
            ImVec2 rectMax = ImGui::GetItemRectMax();
            ImDrawList* draw_list = ImGui::GetForegroundDrawList();
            // Draw a red rectangle around the hovered child slot
            draw_list->AddRect(rectMin, rectMax, IM_COL32(255, 0, 0, 255), 0.0f, 0, 3.0f);

            childToDropOn = child;
            // - set the child as the parent of currently dragged object
            
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
            currentlyDraggedObject = child;
        }

        clickOnSlot(clickedOnItem, -1, child); 

        ImGui::PopStyleColor(2);

        if (child->expanded)
            drawChildSlots(child, clickedOnItem);
        // ---------- End of Child Slot Rendering ---------- //
    }

    ImGui::Unindent(20.0f);

}


// ----- interactions with List ----- //
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

    // std::cout << "[HierarchyInspector] Clicked on slot containing an item : " << index
    //         << " (type of content: " << typeid(*obj).name() << ")" << std::endl;

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
        {
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
            {
                ImVec2 rectMin = ImGui::GetItemRectMin();
                ImVec2 rectMax = ImGui::GetItemRectMax();
                ImDrawList* draw_list = ImGui::GetForegroundDrawList();
                draw_list->AddRect(rectMin, rectMax, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.5f);
                mouseOveringSlotIndex = index;
                childToDropOn = nullptr;
                
            }       
        }

        
        std::string ghostLabel = currentlyDraggedObject->getName();

        if (childToDropOn) {
            ghostLabel += " → " + childToDropOn->getName();
            dropToSlotIndex = -1;
        }

        else if (mouseOveringSlotIndex != -1)
        {
            ghostLabel += " → Slot [" + std::to_string(mouseOveringSlotIndex) + "]";
            dropToSlotIndex = mouseOveringSlotIndex;
        }
        else
        {
            dropToSlotIndex = -1;
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
    ThreeDObject* base = mergedHierarchyList[index];
    EmptyDummy* dummy = dynamic_cast<EmptyDummy*>(base);

    if (dummy && dummy->isDummy())
    {
        glm::mat4 objGlobalModel = obj->getGlobalModelMatrix();
        glm::vec3 objWorldPosition = glm::vec3(objGlobalModel[3]);

        // Unparent the object
        if (ThreeDObject* currentParent = obj->getParent())
        {
            currentParent->removeChild(obj);
            obj->removeParent();
            obj->isParented = false;
        }

        obj->setModelMatrix(objGlobalModel); 
        obj->setOrigin(context->worldCenter); 

        exchangeSlots(obj, index);
    }
    else if (base)
    {
        dropOnObject(base, obj, index);
        dropToSlotIndex = -1;
    }
}

void HierarchyInspector::dropOnObject(ThreeDObject* parent, ThreeDObject* child, int index)
{
    std::cout << "SUPER GIGA TEST DROP ON OBJECT" << std::endl;

    if (child->getParent() == parent)
        return;

    glm::vec3 parentWorldOrigin = glm::vec3(parent->getGlobalModelMatrix() * glm::vec4(parent->getOrigin(), 1.0f));
    glm::mat4 childGlobalBefore = child->getGlobalModelMatrix();

    if (ThreeDObject* oldParent = child->getParent())
    {
        oldParent->removeChild(child);
        child->removeParent();  
        child->isParented = false;
    }

    parent->addChild(child);
    parent->expanded = true;
    child->setParent(parent);
    child->isParented = true;


    glm::mat4 childGlobalAfter = child->getGlobalModelMatrix(); 
    glm::vec3 newLocalOrigin = glm::vec3(glm::inverse(childGlobalAfter) * glm::vec4(parentWorldOrigin, 1.0f));
    child->setOrigin(newLocalOrigin);

    redrawSlotsList();
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

// ------------ redrawing and updates ------------- //

void HierarchyInspector::exchangeSlots(ThreeDObject* obj, int targetIndex)
{
    if (!obj || targetIndex < 0 || targetIndex >= static_cast<int>(mergedHierarchyList.size()))
        return;

    ThreeDObject* target = mergedHierarchyList[targetIndex];
    EmptyDummy* dummy = dynamic_cast<EmptyDummy*>(target);
    if (!dummy || !dummy->isDummy())
        return;

    int objSlot = obj->getSlot();
    int dummySlot = dummy->getSlot(); 

    obj->setSlot(dummySlot);
    dummy->setSlot(objSlot);

    std::swap(mergedHierarchyList[dummySlot], mergedHierarchyList[objSlot]);

    objectsAssignedOnce = true;

    // std::cout << "[exchangeSlots] Swapped object '" << obj->getName()
    //           << "' (now at slot " << dummySlot << ") with dummy (now at slot " << objSlot << ")" << std::endl;
    redrawSlotsList();
}

void HierarchyInspector::redrawSlotsList()
{
    objectsAssignedOnce = false;
    mergedHierarchyList.clear();
    childToDropOn = nullptr;

    for (auto& dummy : emptySlotPlaceholders)
        mergedHierarchyList.push_back(dummy.get());

    std::cout << "[HierarchyInspector] redrawSlotsList triggered." << std::endl;
}


// ------------ other  ------------- //
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

// this function is called externally 
ThreeDObject *HierarchyInspector::getSelectedObject() const
{
    return selectedObjectInHierarchy;
}