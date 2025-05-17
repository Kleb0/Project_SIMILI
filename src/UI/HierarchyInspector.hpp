#pragma ONCE
#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/ThreeDObject.hpp"

class ThreeDWindow;

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
    void unselectobject(ThreeDObject *obj);
    ThreeDObject *getSelectedObject() const;

private:
    OpenGLContext *context = nullptr;
    ThreeDWindow *window = nullptr;
    ThreeDObject *selectedObjectInHierarchy = nullptr;
};