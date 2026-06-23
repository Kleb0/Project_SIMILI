#pragma once

#include "include/cef_browser.h"
#include "../../ThreadSafeIFrameMap.hpp"
#include <atomic>
#include <map>
#include <string>
#include <vulkan/vulkan.h>

class SDL_ApplicationWindow;
class FrameDataCatcher;
class App_Border;
struct SDL_Window;

enum WindowRenderState : int;

namespace SIMILI
{
	namespace Frontend
	{
		class UIManager;
		class IFrameDatas;
	}
}

class PanelResizingLogic
{
public:
	struct PanelLayoutBoundary
	{
		int minX = 0;
		int maxX = 0;
		std::string bottomPanelName;
		std::string leftTopName;
		std::string leftBottomName;
		std::string rightTopName;
		std::string rightBottomName;
		int topRowHeight = 0;
	};

	struct PanelDimensions
	{
		std::string leftTopSelector;
		std::string leftBottomSelector;
		std::string rightTopSelector;
		std::string rightBottomSelector;
		std::string bottomSelector;

		int leftWidth = 0;
		int leftTopHeight = 0;
		int leftBottomHeight = 0;

		int rightWidth = 0;
		int rightTopHeight = 0;
		int rightBottomHeight = 0;

		int topRowHeightScaled = 0;
		int bottomHeight = 0;
	};

	PanelResizingLogic();
	~PanelResizingLogic() = default;

	void setUIManager(SIMILI::Frontend::UIManager* uiManager);
	void setSDLWindow(SDL_ApplicationWindow* sdlWindow);
	void setFrameDatas(SIMILI::Frontend::IFrameDatas* frameDatas);
	void setFrameDataCatcher(FrameDataCatcher* catcher);

	void Set_App_Borders(App_Border& App_Borders, SDL_Window* window, int borderLeft, int borderTop, int borderWidth, int borderHeight);
	void Set_Workspace_Render_Borders(int x, int y, int width, int height);
	void Redraw_Inside_App_Borders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, bool skipTextureRebuild, SDL_Window* window, int referenceWidth, int referenceHeight);

	void cacheUIPanelFrameDatas();
	void syncBrowserPanelLayoutFromCurrentFrames(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight, bool suppressServerNotify = false);
	void syncBrowserFullScreenPanelLayout(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight);
	void finalizeDeferredLayoutRefresh(CefRefPtr<CefBrowser> delayedBrowser);

	PanelLayoutBoundary findColumnsAndBoundaries(const std::map<std::string, IFrameData>& iframeDataMap) const;

	PanelDimensions calculatePanelDimensions(
		const std::map<std::string, IFrameData>& iframeDataMap,
		float scaleX,
		float scaleY,
		int currentHeight,
		int topRowHeight,
		const std::string& leftTopName,
		const std::string& leftBottomName,
		const std::string& rightTopName,
		const std::string& rightBottomName,
		const std::string& bottomPanelName,
		int topBarHeight) const;

	// create a list of panel we will iterate on


private:
	SIMILI::Frontend::UIManager* ui_manager_;
	SDL_ApplicationWindow* sdl_window_;
	SIMILI::Frontend::IFrameDatas* frame_datas_;
	FrameDataCatcher* frame_data_catcher_;
	std::atomic_bool pending_deferred_layout_refresh_;

	int border_left_ = 3;
	int border_top_ = 3;
	int border_width_ = 0;
	int border_height_ = 0;

	int workspace_x_ = 0;
	int workspace_y_ = 0;
	int workspace_width_ = 0;
	int workspace_height_ = 0;
};
