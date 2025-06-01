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
    void renameObject();
    void setPosition();
    void setRotation();
    void setScale();

private:
    ThreeDObject *inspectedObject = nullptr;
    char nameEditBuffer[128] = {0};
    bool isRenaming = false;
};