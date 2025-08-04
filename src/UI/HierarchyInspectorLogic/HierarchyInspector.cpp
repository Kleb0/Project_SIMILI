#define GLM_ENABLE_EXPERIMENTAL
#define IMGUI_ENABLE_ASSERTS
#include "UI/ObjectInspector.hpp"
#include "UI/ThreeDWindow.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>
#include <typeinfo>
#include <filesystem> 
#include "WorldObjects/ThreeDObject.hpp"
#include "Engine/ErrorBox.hpp"

HierarchyInspector::HierarchyInspector()
{
    interactions = std::make_unique<HandleHierarchyInteractions>(this);
    listDrawer = std::make_unique<HierarchyListDrawing>(this);

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


// -------- initialization and management --------- //
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
            window->getSimiliSelector().externalSelect(nullptr);
    }

    // Drag and drop ghost object
    ImGui::End();
}

void HierarchyInspector::drawSlotsList(bool& clickedOnItem) {
    listDrawer->drawSlotsList(clickedOnItem);
}

void HierarchyInspector::drawChildSlots(ThreeDObject* parent, bool& clickedOnItem) {
    listDrawer->drawChildSlots(parent, clickedOnItem);
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

// ------------ interactions with list  ------------- //
void HierarchyInspector::selectObject(ThreeDObject* obj) {
    if (interactions) interactions->selectObject(obj);
}

void HierarchyInspector::clickOnSlot(bool& clickedOnItem, int index, ThreeDObject* obj) {
    if (interactions) interactions->clickOnSlot(clickedOnItem, index, obj);
}

void HierarchyInspector::dragObject(ThreeDObject* obj, int index) {
    if (interactions) interactions->dragObject(obj, index);
}

void HierarchyInspector::dropOnSlot(ThreeDObject* obj, int index) {
    if (interactions) interactions->dropOnSlot(obj, index);
}

void HierarchyInspector::dropOnObject(ThreeDObject* parent, ThreeDObject* child, int index) {
    if (interactions) interactions->dropOnObject(parent, child, index);
}