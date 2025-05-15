#pragma ONCE
#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "UI/ThreeDWindow.hpp"

class HierarchyInspector : public GUIWindow
{

public:
    HierarchyInspector();

    void render() override;
    void setContext(OpenGLContext *ctxt);
    void setThreeDWindow(ThreeDWindow *win);

private:
    OpenGLContext *context = nullptr;
    ThreeDWindow *window = nullptr;
};