#include <glad/glad.h>
#include "UI/MainSoftwareGUI.hpp"
#include <UI/GUIWindow.hpp>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
// #include "Engine/SceneWidgetLinker.hpp"
#include <iostream>

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
        std::cerr << "Erreur lors de l'initialisation de GLFW" << std::endl;
        std::exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Erreur lors de la création de la fenêtre GLFW" << std::endl;
        glfwTerminate();
        std::exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Erreur lors du chargement de GLAD !" << std::endl;
        std::exit(EXIT_FAILURE);
    }
    glfwSwapInterval(1);
}

void MainSoftwareGUI::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
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

        // RENDRE TOUTES LES FENÊTRES AJOUTÉES
        for (auto *win : windows)
        {
            if (win)
                win->render();
        }

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
