#include "my_imgui_config.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "UI/MainSoftwareGUI.hpp"
#include <UI/GUIWindow.hpp>
#include "imgui_internal.h"

#include <fstream>
#include <iostream>
#include <sstream>

GLFWwindow *MainSoftwareGUI::getWindow()
{
    return window;
}

MainSoftwareGUI &MainSoftwareGUI::add(GUIWindow &window)
{
    windows.push_back(&window);
    return *this;
}

MainSoftwareGUI::MainSoftwareGUI(int width, int height, const char *title)
{
    initGLFW(width, height, title);
    initImGui();
}

MainSoftwareGUI::~MainSoftwareGUI()
{
    shutdown();
}
void MainSoftwareGUI::initGLFW(int width, int height, const char *title)
{
    if (!glfwInit())
    {
        std::cerr << "Error during GLFW init" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Error during GLFW creation" << std::endl;
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Error during Glad Creation" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    glfwSwapInterval(1);
}

void MainSoftwareGUI::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    io.IniFilename = nullptr;

    std::ifstream file("../src/resources/default_imgui_layout.ini");
    if (!file)
    {
        std::cerr << "[ERROR] Cannot open default_imgui_layout.ini" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string ini_content = buffer.str();

    ImGui::LoadIniSettingsFromMemory(ini_content.c_str(), ini_content.size());

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void MainSoftwareGUI::run()
{
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("MainDockSpaceWindow", nullptr, window_flags);
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MyMainDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();

        for (auto *win : windows)
            if (win)
                win->render();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

void MainSoftwareGUI::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}
