#pragma once

#include <GLFW/glfw3.h>
#include <vector>
#include <UI/GUIWindow.hpp>

class MainSoftwareGUI
{
public:
    MainSoftwareGUI(int width, int height, const char *title);
    ~MainSoftwareGUI();

    void run();
    // ----------- Expose the GLFW window to the rest of the software --------------
    GLFWwindow *getWindow();

    MainSoftwareGUI &add(GUIWindow &window);

private:
    GLFWwindow *window = nullptr;
    std::vector<GUIWindow *> windows;

    void initGLFW(int width, int height, const char *title);
    void initImGui();
    void shutdown();
};
