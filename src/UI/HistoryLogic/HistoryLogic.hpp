#pragma once
#include <string>
#include "UI/GUIWindow.hpp"
// #include "UI/ObjectInspectorLogic/ObjectInspector.hpp" 

class ObjectInspector;
class OpenGLContext;

class HistoryLogic : public GUIWindow
{
public:
    HistoryLogic();
    ~HistoryLogic() override = default;

    void render() override;

    void setTitle(const std::string& t);
    void setText(const std::string& t);

    void setObjectInspector(ObjectInspector* inspector) { objectInspector = inspector; }
    ObjectInspector* getObjectInspector() const { return objectInspector; }

    void setOpenGLContext(OpenGLContext* ctx) { glctx = ctx; }

private:
    std::string title   = "Scene History";
    std::string content = "Scene History";

    ObjectInspector* objectInspector = nullptr;
    OpenGLContext* glctx = nullptr; 
};
