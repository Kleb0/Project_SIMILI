#pragma once

#include <GLFW/glfw3.h>
#include "Engine/ThreeDSceneDrawer.hpp"

class GameWindow
{
public:
    GameWindow(int width, int height, const char *title);
    ~GameWindow();

    void run();

private:
    GLFWwindow *window;
    ThreeDSceneDrawer scene;

    void initGLFW(int width, int height, const char *title);
    void initImGui();
    void shutdown();

    void renderViewport();
};
