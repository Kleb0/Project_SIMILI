#pragma once

#include "include/cef_browser.h"
#include <atomic>
#include <map>
#include <string>

class SDL_ApplicationWindow;
class FrameDataCatcher;

enum WindowRenderState : int;

namespace SIMILI
{
	namespace Frontend
	{
		class UIManager;
		class FrameDatas;
	}
}

class PanelResizingLogic
{
public:
	PanelResizingLogic();
	~PanelResizingLogic() = default;

	void setUIManager(SIMILI::Frontend::UIManager* uiManager);
	void setSDLWindow(SDL_ApplicationWindow* sdlWindow);
	void setFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas);
	void setFrameDataCatcher(FrameDataCatcher* catcher);

	void cacheUIPanelFrameDatas();
	void syncBrowserPanelLayoutFromCurrentFrames(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight);
	void syncBrowserFullScreenPanelLayout(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight);
	void finalizeDeferredLayoutRefresh(CefRefPtr<CefBrowser> delayedBrowser);

private:
	SIMILI::Frontend::UIManager* ui_manager_;
	SDL_ApplicationWindow* sdl_window_;
	SIMILI::Frontend::FrameDatas* frame_datas_;
	FrameDataCatcher* frame_data_catcher_;
	std::atomic_bool pending_deferred_layout_refresh_;
};
