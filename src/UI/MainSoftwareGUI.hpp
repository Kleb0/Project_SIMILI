#pragma once

#include <GLFW/glfw3.h>
#include <vector>
#include <UI/GUIWindow.hpp>
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include <UI/ObjectInspectorLogic/ObjectInspector.hpp>
#include <WorldObjects/Entities/ThreeDObject.hpp>

class MainSoftwareGUI
{
public:
    MainSoftwareGUI(int width, int height, const char *title);
    ~MainSoftwareGUI();

    void run();
    // ----------- Expose the GLFW window to the rest of the software --------------
    GLFWwindow *getWindow();
    MainSoftwareGUI &add(GUIWindow &window);
    void setThreeDWindow(ThreeDWindow *window) { this->threeDWindow = window; }
    void setObjectInspector(ObjectInspector *inspector) { this->objectInspector = inspector; }

    template <typename T>
    void associate(T &window)
    {
        this->add(window);
    }

private:
    GLFWwindow *window = nullptr;
    std::vector<GUIWindow *> windows;
    bool mustBuildDefaultLayout = false;

    void initGLFW(int width, int height, const char *title);
    void initImGui();
    void tryLoadLayout();
    void autoSaveLayout();
    void mainWindowOptions();
    void popUpModal();
    void shutdown();
    void multiScreenSupport();

    ThreeDObject *threedObject = nullptr;
    ThreeDWindow *threeDWindow = nullptr;
    ObjectInspector *objectInspector = nullptr;
};
