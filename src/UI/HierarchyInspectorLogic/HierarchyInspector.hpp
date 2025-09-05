#pragma once

#include <imgui.h> 
#include "UI/HierarchyInspectorLogic/HandleHierarchyInteractions.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyListDrawing.hpp"
#include "UI/GUIWindow.hpp"
#include "Engine/ThreeDScene.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Entities/EmptyDummy.hpp"
#include <string>
#include <list>
#include <vector>
#include <memory>

class ThreeDWindow;
class ObjectInspector;


struct HierarchyEntry {
    ThreeDObject* object;
    int depth = 0;
};

class HierarchyInspector : public GUIWindow
{
    friend class HandleHierarchyInteractions;
    friend class HierarchyListDrawing;

public:
    HierarchyInspector();

    // -------- Initialization and setting
    void setThreeDScene(ThreeDScene* scene);
    void setThreeDWindow(ThreeDWindow* win);
    void setObjectInspector(ObjectInspector* inspector);
    void setTitle(const std::string& newTitle) { this->title = newTitle; }

    // === Rendering ===
    void render() override;
    // -------- Initialization 
    void drawSlotsList(bool& clickedOnItem); 
    void drawChildSlots(ThreeDObject* parent, bool& clickedOnItem);

    // --------- Management and interactions with slots ----- //
    void clickOnSlot(bool& clickedOnItem, int index, ThreeDObject* obj);
    void dragObject(ThreeDObject* obj, int index);
    void dropOnSlot(ThreeDObject* obj, int index);
    void dropOnObject(ThreeDObject* parent, ThreeDObject* child, int index);    

    // --------- Redrawing and updates
    void exchangeSlots(ThreeDObject* obj, int index);
    void redrawSlotsList();

    // --------- cleaning ----- //

    void clearSelectionState();

    //---------- Selection inside list in HandleHierarchyInteractions.hpp -------------//
    void selectInList(ThreeDObject* obj);
    void selectObject(ThreeDObject* obj);
    void unselectObject(ThreeDObject* obj);
    void selectFromThreeDWindow();
    void multipleSelection(ThreeDObject* obj);
    void clearMultipleSelection();
    ThreeDObject* getSelectedObject() const;

    void renameObject();
    void notifyObjectDeleted(ThreeDObject* obj);

    std::string title;
    int lastSelectedSlotIndex = -1;
    int lastRepositionTargetIndex = -1;
    int mouseOveringSlotIndex = -1;
    int dropToSlotIndex = -1;


private:

    std::unique_ptr<HandleHierarchyInteractions> interactions;
    std::unique_ptr<HierarchyListDrawing> listDrawer;

    ThreeDScene* scene = nullptr;
    ThreeDWindow* window = nullptr;
    ObjectInspector* objectInspector = nullptr;

    ThreeDObject* selectedObjectInHierarchy = nullptr;
    ThreeDObject* objectBeingRenamed = nullptr;
    ThreeDObject* currentlyDraggedObject = nullptr;
    ThreeDObject* pendingDragTarget = nullptr;
    ThreeDObject* lastRepositionedObject = nullptr;
    ThreeDObject* lastSelectedObject = nullptr;
    ThreeDObject* childToDropOn = nullptr;

    char editingBuffer[128] = {0};
    ImFont* unicodeFont = nullptr;

    EmptyDummy emptyDummy;
    std::vector<std::unique_ptr<EmptyDummy>> emptySlotPlaceholders;
    std::list<ThreeDObject*> multipleSelectedObjects;
    std::vector<ThreeDObject*> SlotsList;

    std::vector<ThreeDObject*> inspectorObjects;
    std::vector<std::unique_ptr<EmptyDummy>> dummyList;
    std::vector<ThreeDObject*> mergedHierarchyList; 

    bool shouldReorder = false;
    bool slotsListInitialized = false;
    bool objectsAssignedOnce = false;
    bool hasbeenRedrawn = false;
    bool isOverChild = false;

};