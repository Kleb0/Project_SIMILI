#pragma once
#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/ThreeDObject.hpp"
#include <string>

class ThreeDWindow;
class ObjectInspector;

class HierarchyInspector : public GUIWindow
{

public:
    HierarchyInspector();

    void render() override;
    void setContext(OpenGLContext *ctxt);
    void setThreeDWindow(ThreeDWindow *win);
    void selectInList(ThreeDObject *obj);
    void selectObject(ThreeDObject *obj);
    void selectFromThreeDWindow();
    void unselectObject(ThreeDObject *obj);
    ThreeDObject *getSelectedObject() const;

    std::string title;
    void setTitle(const std::string &newTitle) { this->title = newTitle; }

    void setObjectInspector(ObjectInspector *inspector);
    void renameObject();
    void notifyObjectDeleted(ThreeDObject *obj);

private:
    OpenGLContext *context = nullptr;
    ThreeDWindow *window = nullptr;
    ThreeDObject *selectedObjectInHierarchy = nullptr;
    ObjectInspector *objectInspector = nullptr;
    ThreeDObject *objectBeingRenamed = nullptr;
    char editingBuffer[128] = {0};
};