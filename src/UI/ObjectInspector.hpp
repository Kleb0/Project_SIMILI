#pragma once

#include "UI/GUIWindow.hpp"
#include "WorldObjects/ThreeDObject.hpp"
#include <string>

class ObjectInspector : public GUIWindow
{
public:
    ObjectInspector();

    void render() override;
    void setInspectedObject(ThreeDObject *object);
    void clearInspectedObject();

    std::string title;
    void setTitle(const std::string &newTitle) { this->title = newTitle; }

private:
    ThreeDObject *inspectedObject = nullptr;
};