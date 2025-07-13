#pragma once

#include <imgui.h> 
#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/ThreeDObject.hpp"
#include "WorldObjects/EmptyDummy.hpp"
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
public:
    HierarchyInspector();

    // === Rendering ===
    void render() override;
    // -------- Initialization 
    void drawSlotsList(bool& clickedOnItem); 

    // --------- Management and interactions with slots ----- //
    void clickOnSlot(bool& clickedOnItem, int index, ThreeDObject* obj);
    void dragObject(ThreeDObject* obj, int index);
    void dropOnSlot(ThreeDObject* obj, int index);
    void dropOnObject(ThreeDObject* parent, ThreeDObject* child, int index);
    
    // --------- Redrawing and updates
    void exchangeSlots(ThreeDObject* obj, int index);
    void drawChildSlots(ThreeDObject* parent, bool& clickedOnItem);
    void flattenHierarchyList(std::vector<HierarchyEntry>& flatList, ThreeDObject* obj, int depth);
    void redrawSlotsList();



    void setContext(OpenGLContext* ctxt);
    void setThreeDWindow(ThreeDWindow* win);
    void setObjectInspector(ObjectInspector* inspector);
    void setTitle(const std::string& newTitle) { this->title = newTitle; }


    //---------- Selection inside list -------------//
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

    OpenGLContext* context = nullptr;
    ThreeDWindow* window = nullptr;
    ObjectInspector* objectInspector = nullptr;

    ThreeDObject* selectedObjectInHierarchy = nullptr;
    ThreeDObject* objectBeingRenamed = nullptr;
    ThreeDObject* currentlyDraggedObject = nullptr;
    ThreeDObject* pendingDragTarget = nullptr;
    ThreeDObject* lastRepositionedObject = nullptr;
    ThreeDObject* lastSelectedObject = nullptr;

    char editingBuffer[128] = {0};
    ImFont* unicodeFont = nullptr;

    EmptyDummy emptyDummy;
    std::vector<std::unique_ptr<EmptyDummy>> emptySlotPlaceholders;
    std::list<ThreeDObject*> multipleSelectedObjects;
    std::vector<ThreeDObject*> SlotsList;
    // new lists 
    std::vector<ThreeDObject*> inspectorObjects;
    std::vector<std::unique_ptr<EmptyDummy>> dummyList;
    std::vector<ThreeDObject*> mergedHierarchyList; 

    bool shouldReorder = false;
    bool slotsListInitialized = false;
    bool objectsAssignedOnce = false;
    bool hasbeenRedrawn = false;

};