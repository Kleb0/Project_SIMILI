#pragma once

#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "viewportLogic/overlay_viewport.hpp"
#include "viewportLogic/Keymanagement/IFrameMouseDetector.hpp"
#include "viewportLogic/FrameDatas/FrameDatas.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_State.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_Above_Overlay_State.hpp"
#include "viewportLogic/Keymanagement/MouseStates/Mouse_Outside_Overlay_State.hpp"
#include <list>
#include <sstream>
#include <memory>
#include <windows.h>
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>

class SimpleWindowDelegate;
class Overlay_HTML_Texture_Renderer;

namespace SIMILI 
{
	namespace Input 
	{
		class MouseControlToOverlay;
	}
}

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

	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, 
	const CefString& errorText, const CefString& failedUrl) override;

	// keyboard events
	virtual bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
		CefEventHandle os_event, bool* is_keyboard_shortcut) override;
	virtual bool OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
		CefEventHandle os_event) override;

	void CloseAllBrowsers(bool force_close);
	void createOverlayViewport(HWND parent_hwnd);
	void updateOverlayPosition();
	bool isOverlayRenderingEnabled() const;
	void enableSlotTextureRendering(bool enable);
	void enableCompositeTestRenderer(bool enable);
	OverlayViewport* getOverlay() { return overlay_viewport_.get(); }
	HWND getParentHWND() const { return parent_hwnd_; }
	SIMILI::Input::IFrameMouseDetector* getIFrameMouseDetector() { return iframe_mouse_detector_; }
	
	void setThreeDScene(ThreeDScene* scene) { three_d_scene_ = scene; }
	ThreeDScene* getThreeDScene() const { return three_d_scene_; }
	
	void setSceneObjects(OpenGLContext* renderer, ThreeDScene* scene, Camera* camera, Mesh** cubeMesh);
	void initializeSceneObjects();
	void initializeFrameDatas(SimpleWindowDelegate* windowDelegate);
	void reinitializeSingleObject(ThreeDObject* obj);
	void notifySceneChanged();
	void setIFrameMouseDetector(SIMILI::Input::IFrameMouseDetector* detector);
	void setWindowDelegate(SimpleWindowDelegate* delegate) { window_delegate_ = delegate; }
	SimpleWindowDelegate* getWindowDelegate() const { return window_delegate_; }
	
	void logIFrameSizes();
	void updatePanelBoundsFromStocker();
	
	void captureIFramePositions();
	void updateIFrameMouseDetectorFromFrameDatas();
	SIMILI::Frontend::FrameDatas* getFrameDatas() { return frame_datas_; }
	
	void transitionMouseState(const std::string& regionName);
	SIMILI::Input::Mouse_State* getCurrentMouseState() const { return current_mouse_state_; }
	
	// Mouse state getters
	SIMILI::Input::Mouse_Above_Overlay_State* getAboveOverlayState() const { return above_overlay_state_; }
	SIMILI::Input::Mouse_Outside_Overlay_State* getOutsideOverlayState() const { return outside_overlay_state_; }
	
	// Last detected region name accessors
	std::string getLastDetectedRegionName() const { return last_detected_region_name_; }
	void setLastDetectedRegionName(const std::string& regionName) { last_detected_region_name_ = regionName; }
	
	SIMILI::Input::MouseControlToOverlay* getMouseControlToOverlay() const { return mouse_control_to_overlay_; }	

	ID3D11Device* getD3D11Device() const { return d3d11_device_; }
	ID2D1Factory1* getD2D1Factory() const { return d2d_factory_; }
	ID2D1Device* getD2D1Device() const { return d2d_device_; }
	IDXGIDevice1* getDXGIDevice() const { return dxgi_device_; }

	friend VOID CALLBACK RenderTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

	void CallTestFromServer();

private:
	static UIHandler* s_instance_;

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
	
	// Mouse detection
	SIMILI::Input::IFrameMouseDetector* iframe_mouse_detector_;
	SIMILI::Frontend::FrameDatas* frame_datas_;
	SimpleWindowDelegate* window_delegate_;
	
	// Mouse states
	SIMILI::Input::Mouse_State* current_mouse_state_;
	SIMILI::Input::Mouse_Above_Overlay_State* above_overlay_state_;
	SIMILI::Input::Mouse_Outside_Overlay_State* outside_overlay_state_;
	std::string last_detected_region_name_;
	
	SIMILI::Input::MouseControlToOverlay* mouse_control_to_overlay_;
	
	Overlay_HTML_Texture_Renderer* slot_texture_renderer_;
	Overlay_HTML_Texture_Renderer* composite_test_renderer_;

	ID3D11Device* d3d11_device_;
	ID3D11DeviceContext* d3d11_device_context_;
	IDXGIDevice1* dxgi_device_;
	ID2D1Factory1* d2d_factory_;
	ID2D1Device* d2d_device_;

	IMPLEMENT_REFCOUNTING(UIHandler);
};
