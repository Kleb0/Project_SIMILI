#pragma once

#include <GLFW/glfw3.h>
#include <vector>
#include <UI/GUIWindow.hpp>
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include <UI/ObjectInspectorLogic/ObjectInspector.hpp>
#include "UI/HistoryLogic/HistoryLogic.hpp"
#include <WorldObjects/Entities/ThreeDObject.hpp>
#include "UI/ContextualMenu/ContextualMenu.hpp"
#include "Engine/SaveLoadSystem/Save_Scene.hpp"
#include "UI/EdgeLoopControl/EdgeLoopControl.hpp"

class OptionsMenuContent;
class EdgeLoopControl;
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
	void setScene(ThreeDScene* scene);

	template <typename T>
	void associate(T &window)
	{
		this->add(window);
	}

	void setContextualMenu(ContextualMenu* menu);
	void setOptionsMenu(OptionsMenuContent* menu) { optionsMenu = menu; }
	void setEdgeLoopControl(EdgeLoopControl* control);
	void DisplayEdgeLoopControl(ThreeDWindow *window);

private:
	GLFWwindow *window = nullptr;
	std::vector<GUIWindow *> windows;
	bool mustBuildDefaultLayout = false;

	void initGLFW(int width, int height, const char *title);
	void initImGui();
	void tryLoadLayout();
	void autoSaveLayout();
	void shutdown();
	void multiScreenSupport();

	ThreeDObject *threedObject = nullptr;
	ThreeDWindow *threeDWindow = nullptr;
	ObjectInspector *objectInspector = nullptr;
	ContextualMenu *contextualMenu = nullptr;
	OptionsMenuContent *optionsMenu = nullptr;
	EdgeLoopControl *edgeLoopControl = nullptr;
	ThreeDScene* sceneRef = nullptr;
	
};
