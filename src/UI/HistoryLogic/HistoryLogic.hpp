
#pragma once
#include <string>
#include "UI/GUIWindow.hpp"
// #include "UI/ObjectInspectorLogic/ObjectInspector.hpp" 

class ObjectInspector;
class ThreeDScene;
class ThreeDWindow;
class HierarchyInspector;
class ThreeDObject;

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

    void setThreeDScene(ThreeDScene* scene) { this->scene = scene; }


    void setThreeDWindow(ThreeDWindow* w) { threeDWindow = w; }
    ThreeDWindow* getThreeDWindow() const { return threeDWindow; }

    void setHierarchyInspector(HierarchyInspector* h) { hierarchyInspector = h; }
    HierarchyInspector* getHierarchyInspector() const { return hierarchyInspector; }

    
private:
    std::string title = "Scene History";
    std::string content = "Scene History";

    ObjectInspector* objectInspector = nullptr;
    ThreeDScene* scene = nullptr;
    ThreeDWindow* threeDWindow = nullptr;
    HierarchyInspector* hierarchyInspector = nullptr;
};
