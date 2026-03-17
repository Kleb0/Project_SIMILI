#pragma once

#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "SDL_ApplicationWindow.hpp"
#include "CEF_Drawer.hpp"
#include "viewportLogic/overlay_viewport.hpp"
#include "viewportLogic/FrameDatas/FrameDatas.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_State.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_Above_Overlay_State.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_Outside_Overlay_State.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_Above_UI_Panel_State.hpp"
#include "viewportLogic/HTMLTextureRenderer/Overlay_HTML_Texture_Renderer.hpp"
#include <list>
#include <sstream>
#include <memory>
#include <SDL3/SDL.h>
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>

class SimpleWindowDelegate;

struct IFrameData
{
	std::string name;
	int x;
	int y;
	int width;
	int height;
	int clientX;
	int clientY;
};

namespace SIMILI 
{
	namespace Input 
	{
		class MouseController;
	}
}

class VKScene;
class VKContext;
class Camera;
class Mesh;

class UIHandler : public CefApp,
				  public CefClient, 
				  public CefBrowserProcessHandler,
				  public CefRenderProcessHandler,
				  public CefDisplayHandler,
				  public CefLifeSpanHandler, 
				  public CefLoadHandler, 
				  public CefKeyboardHandler
{
public:
	explicit UIHandler();
	~UIHandler();

	static UIHandler* getInstance();
	static void setInstance(UIHandler* handler);

	// CefApp methods
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;
	virtual void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;

	// CefBrowserProcessHandler methods
	virtual void OnContextInitialized() override;

	// CefRenderProcessHandler methods
	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
	virtual void OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

	// CefClient methods
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override;
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override;

	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, 
	const CefString& errorText, const CefString& failedUrl) override;
	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;

	// keyboard events
	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
	CefEventHandle os_event, bool* is_keyboard_shortcut) override;
	
	virtual bool OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
	CefEventHandle os_event) override;

	void CloseAllBrowsers(bool force_close);
	bool isOverlayRenderingEnabled() const;
	void enableSlotTextureRendering(bool enable);
	void enableCompositeTestRenderer(bool enable);
	OverlayViewport* getOverlay() { return overlay_viewport_; }
	SDL_Window* getParentWindow() const { return parent_window_; }

	void setParentWindow(SDL_Window* window) { parent_window_ = window; }
	void set_Overlay_Viewport(OverlayViewport* viewport);
	void Set_SDLParent(SDL_ApplicationWindow* parentWindow);
	void Set_DOM(CEF_Drawer* drawer);
	
	void setVKScene(VKScene* scene) { vk_scene_ = scene; }
	VKScene* getVKScene() const { return vk_scene_; }
	
	void startRenderTimer();
	
	void initializeSceneObjects();
	void initializeFrameDatas(SimpleWindowDelegate* windowDelegate);
	void reinitializeSingleObject(ThreeDObject* obj);
	void notifySceneChanged();
	void set_MouseControl(SIMILI::Input::MouseController* mouseControl);
	void setWindowDelegate(SimpleWindowDelegate* delegate) { window_delegate_ = delegate; }
	SimpleWindowDelegate* getWindowDelegate() const { return window_delegate_; }
	
	
	void captureIFramePositions();
	SIMILI::Frontend::FrameDatas* getFrameDatas() { return frame_datas_; }

	void setFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas) { frame_datas_ = frameDatas; }
	
	// Camera operation lock
	bool isCameraOperationLocked() const { return is_camera_operation_locked_; }
	void setCameraOperationLocked(bool locked) { is_camera_operation_locked_ = locked; }
	
	SIMILI::Input::MouseController* getMouseController() const { return mouse_controller_; }	
	
	Overlay_HTML_Texture_Renderer* getSlotTextureRenderer() const { return slot_texture_renderer_; }
	Overlay_HTML_Texture_Renderer* getCompositeTestRenderer() const { return composite_test_renderer_; }
	
	CEF_Drawer* getCEFDrawer() const { return cef_drawer_; }
	void setCEFDrawer(CEF_Drawer* drawer) { cef_drawer_ = drawer; }

	ID3D11Device* getD3D11Device() const { return d3d11_device_; }
	ID2D1Factory1* getD2D1Factory() const { return d2d_factory_; }
	ID2D1Device* getD2D1Device() const { return d2d_device_; }
	IDXGIDevice1* getDXGIDevice() const { return dxgi_device_; }

	friend Uint32 SDLCALL RenderTimerProc(void* param, SDL_TimerID timerID, Uint32 interval, SDL_ApplicationWindow* parentWindow, CEF_Drawer* dom);

	void CallTestFromServer();

private:
	static UIHandler* s_instance_;

	typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
	BrowserList browser_list_;
	OverlayViewport* overlay_viewport_;
	SDL_ApplicationWindow* parent_sdl_window_;
	SDL_Window* parent_window_;
	SDL_Window* window_handle_;
	SDL_TimerID timer_id_;
	VKScene* vk_scene_;
	

	VKContext* vk_renderer_;
	Camera* main_camera_;
	Mesh** cube_mesh_ptr_;
	bool scene_initialized_;
	
	Uint64 last_viewport_update_time_;
	int last_viewport_x_;
	int last_viewport_y_;
	int last_viewport_width_;
	int last_viewport_height_;
	
	CefRefPtr<CefMessageRouterRendererSide> render_message_router_;
	
	// Mouse control
	SIMILI::Frontend::FrameDatas* frame_datas_;
	SimpleWindowDelegate* window_delegate_;
	
	SIMILI::Input::MouseController* mouse_controller_;
	
	// Camera operation lock
	bool is_camera_operation_locked_;
	
	Overlay_HTML_Texture_Renderer* slot_texture_renderer_;
	Overlay_HTML_Texture_Renderer* composite_test_renderer_;
	CEF_Drawer* cef_drawer_;

	ID3D11Device* d3d11_device_;
	ID3D11DeviceContext* d3d11_device_context_;
	IDXGIDevice1* dxgi_device_;
	ID2D1Factory1* d2d_factory_;
	ID2D1Device* d2d_device_;

public:
	std::map<std::string, IFrameData> iframe_data_map_;
	const std::map<std::string, IFrameData>& getAllIFrames() const { return iframe_data_map_; }
	SDL_ApplicationWindow* getSDLParent() { return parent_sdl_window_; }

private:
	IMPLEMENT_REFCOUNTING(UIHandler);
};
