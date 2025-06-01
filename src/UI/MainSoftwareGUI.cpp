#include "my_imgui_config.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "UI/MainSoftwareGUI.hpp"
#include <UI/GUIWindow.hpp>
#include "imgui_internal.h"
#include "UI/UiCreator.hpp"
#include "UI/Uidocking.hpp"
#include "UI/ContextualMenu.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

#include "tinyfiledialogs.h"

extern std::filesystem::path gExecutableDir;

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

void MainSoftwareGUI::tryLoadLayout()
{
    std::ifstream autosaveFile((gExecutableDir / "autosave_layout.ini").string());
    if (autosaveFile)
    {
        std::stringstream autosaveBuffer;
        autosaveBuffer << autosaveFile.rdbuf();
        const std::string autosaveIniContent = autosaveBuffer.str();
        ImGui::LoadIniSettingsFromMemory(autosaveIniContent.c_str(), autosaveIniContent.size());
        std::cout << "[INFO] UI layout loaded from autosave_layout.ini" << std::endl;
        return;
    }

    ImGui::LoadIniSettingsFromMemory("", 0);
    mustBuildDefaultLayout = true;
}

void MainSoftwareGUI::autoSaveLayout()
{
    const char *currentIniData = ImGui::SaveIniSettingsToMemory();
    fs::path autosavePath = gExecutableDir / "autosave_layout.ini";
    std::ofstream out(autosavePath, std::ios::binary | std::ios::trunc);
    if (out)
    {
        out.write(currentIniData, std::strlen(currentIniData));
        std::cout << "[UI_CREATOR_INFO] Layout autosaved to: " << autosavePath << std::endl;
    }
    else
    {
        std::cerr << "[ERROR] Failed to autosave layout to: " << autosavePath << std::endl;
    }
}

void MainSoftwareGUI::mainWindowOptions()
{
    if (ImGui::BeginMenu("Options"))
    {
        if (ImGui::MenuItem("Save current UI as..."))
        {
            const char *filterPatterns[1] = {"*.ini"};
            const char *savePath = tinyfd_saveFileDialog(
                "Save UI Layout",
                "custom_layout.ini",
                1,
                filterPatterns,
                "INI Layout Files");

            if (savePath)
            {
                const char *ini_data = ImGui::SaveIniSettingsToMemory();
                std::ofstream out(savePath, std::ios::binary | std::ios::trunc);
                if (out)
                {
                    out.write(ini_data, std::strlen(ini_data));
                    std::cout << "[UI_CREATOR_INFO] Layout saved to: " << savePath << std::endl;
                }
                else
                {
                    std::cerr << "[ERROR] Failed to save layout to: " << savePath << std::endl;
                }
            }
        }

        if (ImGui::MenuItem("Load UI layout..."))
        {
            const char *filterPatterns[1] = {"*.ini"};
            const char *loadPath = tinyfd_openFileDialog(
                "Load UI Layout",
                "",
                1,
                filterPatterns,
                "INI Layout Files",
                0);

            if (loadPath)
            {
                UiCreator::loadLayoutFromFile(loadPath);
                UiCreator::saveLastLayoutPath(loadPath);
            }
        }

        ImGui::EndMenu();
    }
}

void MainSoftwareGUI::popUpModal()
{
    if (ImGui::BeginPopupModal("SaveLayoutPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char filenameBuffer[128] = "custom_layout.ini";
        static std::string saveDirectory = (fs::current_path().parent_path() / "src" / "resources").string();
        static char directoryBuffer[256];
        static bool initialized = false;

        if (!initialized)
        {
            std::strncpy(directoryBuffer, saveDirectory.c_str(), sizeof(directoryBuffer));
            initialized = true;
        }

        ImGui::InputText("Filename", filenameBuffer, IM_ARRAYSIZE(filenameBuffer));
        ImGui::InputText("Directory", directoryBuffer, IM_ARRAYSIZE(directoryBuffer));

        if (ImGui::Button("Save"))
        {
            std::string fullPath = std::string(directoryBuffer) + "/" + std::string(filenameBuffer);
            const char *ini_data = ImGui::SaveIniSettingsToMemory();
            std::ofstream out(fullPath, std::ios::binary | std::ios::trunc);
            if (out)
            {
                out.write(ini_data, std::strlen(ini_data));
                std::cout << "[UI_CREATOR_INFO] Layout saved to: " << fullPath << std::endl;
            }
            else
            {
                std::cerr << "[ERROR] Failed to save layout to: " << fullPath << std::endl;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
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

    ImGui::StyleColorsDark();
    multiScreenSupport();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    tryLoadLayout();
}

void MainSoftwareGUI::run()
{
    ContextualMenu contextualMenu;
    static bool showSaveLayoutPopup = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            contextualMenu.show();
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            contextualMenu.hide();
        }

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

        mainWindowOptions();

        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MyMainDockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (mustBuildDefaultLayout)
        {
            Uidocking::SetupDefaultDockspace(dockspace_id);
            mustBuildDefaultLayout = false;
        }

        ImGui::End();

        popUpModal();

        for (auto *win : windows)
            if (win)
                win->render();

        contextualMenu.render();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }
}

void MainSoftwareGUI::shutdown()
{

    autoSaveLayout();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void MainSoftwareGUI::multiScreenSupport()
{
    ImGuiIO &io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
}