#define GLM_ENABLE_EXPERIMENTAL
#include "my_imgui_config.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "UI/MainSoftwareGUI.hpp"
#include <UI/GUIWindow.hpp>
#include "imgui_internal.h"
#include "UI/UIDocking/UiCreator.hpp" 
#include "UI/UIDocking/Uidocking.hpp"
#include "UI/OptionMenu/OptionsMenu.hpp"
#include "UI/ContextualMenu/ContextualMenu.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

#include "Engine/ErrorBox.hpp"
#include "Engine/SaveLoadSystem/Save_Scene.hpp"

namespace fs = std::filesystem;

#include "tinyfiledialogs.h"

extern std::filesystem::path gExecutableDir;

GLFWwindow *MainSoftwareGUI::getWindow()
{
	return window;
}

MainSoftwareGUI &MainSoftwareGUI::add(GUIWindow &w)
{
	windows.push_back(&w);
	return *this;
}

void MainSoftwareGUI::setContextualMenu(ContextualMenu* menu)
{
	contextualMenu = menu;
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




void MainSoftwareGUI::initGLFW(int width, int height, const char *title)
{
	if (!glfwInit())
	{
		std::cerr << "Error during GLFW init" << std::endl;
		std::exit(EXIT_FAILURE);
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
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

	std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
	std::cout << "Renderer: " << glGetString(GL_RENDERER) << std::endl;
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

	for (auto *win : windows)
	{
		auto *hierarchy = dynamic_cast<HierarchyInspector *>(win);
	}

	static bool showSaveLayoutPopup = false;

	while (!glfwWindowShouldClose(window))
	{

		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		if (contextualMenu)
		{
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
				contextualMenu->show();

			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				ImGuiWindow *hoveredWindow = GImGui->HoveredWindow;
				if (!hoveredWindow || std::string(hoveredWindow->Name) != "##ContextualMenu")
					contextualMenu->hide();
			}
		}

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
		const ImGuiViewport *viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + 20));
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowViewport(viewport->ID);

		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("MainDockSpaceWindow", nullptr, window_flags);

		if (ImGui::BeginMenuBar()) 
		{
			const char* buttonText = " Options ";
			ImVec2 textSize = ImGui::CalcTextSize(buttonText);
			float paddingX = 16.0f;
			float paddingY = 4.0f;
			ImVec2 buttonSize(textSize.x + 2 * paddingX, textSize.y + 2 * paddingY);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.05f, 0.15f, 0.35f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.20f, 0.45f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.15f, 0.35f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(paddingX, paddingY));

			if (ImGui::Button(buttonText, buttonSize)) 
			{
				if (optionsMenu) 
				{
					optionsMenu->optionsMenuHasbeenClicked = true;
					optionsMenu->optionsButtonPos = ImGui::GetItemRectMin();
				}
				ImGui::OpenPopup("OptionsMenuPopup");
			}
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(4);
			ImGui::EndMenuBar();
		}

		ImGui::PopStyleVar(2);

		ImGuiID dockspace_id = ImGui::GetID("MyMainDockspace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None); 

		if (mustBuildDefaultLayout)
		{
			Uidocking::SetupDefaultDockspace(dockspace_id);
			mustBuildDefaultLayout = false;
		}

		ImGui::End();

		for (auto *win : windows)
			if (win)
				win->render();

		if (contextualMenu)
			contextualMenu->render();

		if (optionsMenu && optionsMenu->optionsMenuHasbeenClicked)
		{
			ImGui::OpenPopup("OptionsMenuPopup");
			if (ImGui::BeginPopup("OptionsMenuPopup"))
			{
				optionsMenu->render([this](const std::string& path){ this->saveActiveScene(path); }, sceneRef);
				ImGui::EndPopup();
			}
		}


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

void MainSoftwareGUI::setScene(ThreeDScene* scene) 
{
	sceneRef = scene;
}

void MainSoftwareGUI::saveActiveScene(const std::string& filePath) 
{
	if (sceneRef) 
	{
		SaveScene saveScene(sceneRef->getSceneData()); 
		saveScene.saveToJson(filePath);
	} 
	else 
	{
		throw std::runtime_error("No active scene to save.");
	}
}