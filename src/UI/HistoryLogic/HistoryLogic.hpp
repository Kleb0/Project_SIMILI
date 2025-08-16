#pragma once
#include <string>
#include "UI/GUIWindow.hpp"
#include "UI/ObjectInspectorLogic/ObjectInspector.hpp" 

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

private:
    std::string title   = "Scene History";
    std::string content = "Scene History";

    ObjectInspector* objectInspector = nullptr;
};
