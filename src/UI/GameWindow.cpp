#include <glad/glad.h>
#include "UI/GameWindow.hpp"
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "Engine/ThreeDWorldView.hpp"
#include <iostream>

ThreeDWorldView viewport;

GameWindow::GameWindow(int width, int height, const char *title)
{
    initGLFW(width, height, title);
    initImGui();
}

GameWindow::~GameWindow()
{
    shutdown();
}

void GameWindow::initGLFW(int width, int height, const char *title)
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

void GameWindow::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void GameWindow::run()
{
    viewport.initialize();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Game Window");
        ImGui::Text("Bienvenue !");
        ImGui::End();

        ImGui::Begin("Viewport 3D");
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        int viewportWidth = static_cast<int>(viewportSize.x);
        int viewportHeight = static_cast<int>(viewportSize.y);

        viewport.resize(viewportWidth, viewportHeight);
        viewport.render();

        ImGui::Image(
            (ImTextureID)(uintptr_t)viewport.getTexture(),
            ImVec2(viewportWidth, viewportHeight),
            ImVec2(0, 1), ImVec2(1, 0));

        ImGui::End();

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

void GameWindow::shutdown()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}
