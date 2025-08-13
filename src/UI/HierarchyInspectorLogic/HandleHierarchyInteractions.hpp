#pragma once

#include "WorldObjects/Entities/ThreeDObject.hpp"

class HierarchyInspector;

class HandleHierarchyInteractions {
public:
    HandleHierarchyInteractions(HierarchyInspector* inspector);

    void clickOnSlot(bool& clickedOnItem, int index, ThreeDObject* obj);
    void dragObject(ThreeDObject* obj, int index);
    void dropOnSlot(ThreeDObject* obj, int index);
    void dropOnObject(ThreeDObject* parent, ThreeDObject* child, int index);
    void selectObject(ThreeDObject* obj);
    void selectInList(ThreeDObject* obj);
    void multipleSelection(ThreeDObject* obj);

private:
    HierarchyInspector* inspector;
};

