#pragma once

#include <GLFW/glfw3.h>
#include <vector>
#include <UI/GUIWindow.hpp>
#include <UI/ThreeDWindow.hpp>

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
    ThreeDWindow *threeDWindow = nullptr;
};
