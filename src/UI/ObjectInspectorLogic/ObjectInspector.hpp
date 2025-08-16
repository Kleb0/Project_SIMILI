#pragma once

#include "UI/GUIWindow.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include <string>
#include <iostream>
#include <list>

class ObjectInspector : public GUIWindow
{
public:
    ObjectInspector();

    void render() override;
    void setInspectedObject(ThreeDObject *object);
    void clearInspectedObject();
    void setMultipleInspectedObjects(const std::list<ThreeDObject *> &objects);
    void clearMultipleInspectedObjects();

    void dipslayGlobaleCoordinates(ThreeDObject* object);

    std::string title;
    void setTitle(const std::string &newTitle) { this->title = newTitle; }
    void renameObject();
    void setPosition();
    void setRotation();
    void setScale();

    bool showGlobalOrigin = false;

    ThreeDObject* getInspectedObject() const { return inspectedObject; }

private:
    ThreeDObject *inspectedObject = nullptr;
    std::list<ThreeDObject *> multipleInspectedObjects;
    char nameEditBuffer[128] = {0};
    bool isRenaming = false;
};