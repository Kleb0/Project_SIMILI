#pragma once

#include <imgui.h>
#include "WorldObjects/ThreeDObject.hpp"
#include <vector>
#include <list>
#include <memory>

class HierarchyInspector;
class EmptyDummy;

class HierarchyListDrawing {
public:
    HierarchyListDrawing(HierarchyInspector* inspector);

    void drawSlotsList(bool& clickedOnItem);
    void drawChildSlots(ThreeDObject* parent, bool& clickedOnItem);

private:
    HierarchyInspector* inspector = nullptr;
};
