#pragma once

#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "viewportLogic/overlay_viewport.hpp"
#include <list>
#include <sstream>
#include <memory>

// Forward declarations
class ThreeDScene;
class OpenGLContext;
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

	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, 
	const CefString& errorText, const CefString& failedUrl) override;

	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
		CefEventHandle os_event, bool* is_keyboard_shortcut) override;
	virtual bool OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
		CefEventHandle os_event) override;

	void CloseAllBrowsers(bool force_close);
	void createOverlayViewport(HWND parent_hwnd);
	void updateOverlayPosition();
	void startRenderTimer();
	void stopRenderTimer();
	bool isOverlayRenderingEnabled() const;
	void enableSlotTextureRendering(bool enable);
	OverlayViewport* getOverlay() { return overlay_viewport_.get(); }
	HWND getParentHWND() const { return parent_hwnd_; }
	
	void setThreeDScene(ThreeDScene* scene) { three_d_scene_ = scene; }
	ThreeDScene* getThreeDScene() const { return three_d_scene_; }
	
	void setSceneObjects(OpenGLContext* renderer, ThreeDScene* scene, Camera* camera, Mesh** cubeMesh);
	void initializeSceneObjects();
	void reinitializeSingleObject(ThreeDObject* obj);
	void notifySceneChanged();
	
	static LRESULT CALLBACK ParentWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
	typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
	BrowserList browser_list_;
	std::unique_ptr<OverlayViewport> overlay_viewport_;
	HWND parent_hwnd_;
	UINT_PTR timer_id_;
	ThreeDScene* three_d_scene_;
	

	OpenGLContext* renderer_;
	Camera* main_camera_;
	Mesh** cube_mesh_ptr_;
	bool scene_initialized_;
	
	DWORD last_viewport_update_time_;
	int last_viewport_x_;
	int last_viewport_y_;
	int last_viewport_width_;
	int last_viewport_height_;
	
	CefRefPtr<CefMessageRouterRendererSide> render_message_router_;

	IMPLEMENT_REFCOUNTING(UIHandler);
};
