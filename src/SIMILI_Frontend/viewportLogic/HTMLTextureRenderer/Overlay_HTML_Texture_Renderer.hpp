#pragma once

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include <windows.h>
#include <glad/glad.h>
#include <string>
#include <atomic>
#include <functional>
#include "../../ThirdParty/CEF/cef_binary/include/cef_client.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_render_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_life_span_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_display_handler.h"

class Overlay_HTML_Texture_Renderer : public CefClient, public CefRenderHandler, public CefLifeSpanHandler, public CefDisplayHandler {
public:
	Overlay_HTML_Texture_Renderer(const std::string& htmlUrl);
	~Overlay_HTML_Texture_Renderer();

	bool create(HWND parent, int x, int y, int width, int height);
	void destroy();

	void setPosition(int x, int y, int width, int height);
	void show(bool visible);
	bool isVisible() const;

	void render();
	void enableRendering(bool enable);
	
	void SetParentByName(const std::string& parentName);
	void UpdateParentData(int parentX, int parentY, int parentWidth, int parentHeight, float dpiScale = 1.0f);
	bool hasParent() const { return has_parent_; }
	bool isRenderingEnabled() const { return rendering_enabled_; }
	
	void setTopLeft();
	void setTopRight();
	void setBottomLeft();
	void setBottomRight();
	void setMiddleLeft();
	void setMiddleRight();
	void setCenterize();

	void setCanReceiveInputs(bool canReceive) { can_receive_inputs_ = canReceive; }
	bool canReceiveInputs() const { return can_receive_inputs_; }

	void updateHTMLTextureSize(int width, int height);

	HWND getHandle() const { return hwnd_; }
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }

	CefRefPtr<CefBrowser> getBrowser() { return browser_; }

	std::string getInstanceID() const { return instance_id_; }

	void invalidate();

	void sendKeyEvent(const CefKeyEvent& event);

	void setOnBrowserCreatedCallback(std::function<void(CefRefPtr<CefBrowser>)> callback) {
		on_browser_created_callback_ = callback;
	}

	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

	virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
	virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
						const RectList& dirtyRects, const void* buffer,
						int width, int height) override;

	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
								   cef_log_severity_t level,
								   const CefString& message,
								   const CefString& source,
								   int line) override;

	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

	bool isBrowserClosed() const { return browser_closed_; }

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void initializeOpenGL(HGLRC shareContext = nullptr);
	void createQuad();
	void renderQuad();
	void createBrowser();
	void injectScrollbarEliminationCSS();
	void updateTexture(const void* buffer, int width, int height);
	static std::string generateUniqueID();

	std::string instance_id_;
	std::string html_url_;

	HWND hwnd_;
	HWND parent_;
	HDC hdc_;
	HGLRC gl_context_;

	int width_;
	int height_;

	GLuint vao_;
	GLuint vbo_;
	GLuint shader_program_;

	float color_r_;
	float color_g_;
	float color_b_;
	float color_a_;

	GLuint texture_id_;
	int texture_width_;
	int texture_height_;
	CefRefPtr<CefBrowser> browser_;

	bool rendering_enabled_;
	bool html_browser_ready_;
	bool can_receive_inputs_;

	std::atomic<bool> being_destroyed_{false};
	std::atomic<bool> browser_closed_{false};

	std::function<void(CefRefPtr<CefBrowser>)> on_browser_created_callback_;
	
	bool has_parent_;
	std::string parent_name_;
	int parent_x_;
	int parent_y_;
	int parent_width_;
	int parent_height_;
	float parent_dpi_scale_;

	IMPLEMENT_REFCOUNTING(Overlay_HTML_Texture_Renderer);
};
