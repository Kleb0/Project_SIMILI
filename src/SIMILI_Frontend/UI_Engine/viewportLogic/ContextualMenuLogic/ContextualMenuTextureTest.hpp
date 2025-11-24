#pragma once

#include <glad/glad.h>
#include <windows.h>
#include "../../ThirdParty/CEF/cef_binary/include/cef_client.h"

class ContextualMenuTextureTest 
{
public:
	ContextualMenuTextureTest();
	~ContextualMenuTextureTest();
	
	// Initialize the red texture
	void initialize(int width, int height);
	
	// Render the red texture as an overlay
	void render(int viewport_width, int viewport_height);
	
	// Update texture size if needed
	void resize(int width, int height);
	
	// Cleanup
	void cleanup();
	
	// Get texture ID for external use
	GLuint getTextureID() const { return texture_id_; }
	
	// Visibility control
	void show() { visible_ = true; }
	void hide() { visible_ = false; }
	void toggleVisibility() { visible_ = !visible_; }
	bool isVisible() const { return visible_; }
	
	// CEF browser management
	void setBrowser(CefRefPtr<CefBrowser> browser) { browser_ = browser; }
	
	// Mouse input forwarding to CEF
	void sendMouseMove(int x, int y);
	void sendMouseClick(int x, int y, bool is_left_button);
	
private:
	void createTexture();
	void createShaderProgram();
	void createQuad();
	
	GLuint texture_id_;
	GLuint shader_program_;
	GLuint vao_;
	GLuint vbo_;
	
	int width_;
	int height_;
	
	bool initialized_;
	bool visible_;
	
	CefRefPtr<CefBrowser> browser_;
};
