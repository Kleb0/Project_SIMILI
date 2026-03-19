#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <string>

class UIPanel
{
public:
	UIPanel();
	~UIPanel();

	bool initialize(const std::string& panelName);
	void shutdown();
	void updateFromFrameData(const SIMILI::Frontend::IFrameScreenData& frameData, SDL_Window* window, bool skipTextureRebuild = false);
	void draw(int drawableWidth, int drawableHeight);
	void forceTextureRebuild();

	const std::string& getName() const { return name_; }

private:
	bool createTexture();
	bool createShaderProgram();
	void updateTextureRegion();
	void updateGeometry(int drawableWidth, int drawableHeight);

	std::string name_;
	GLuint texture_id_;
	GLuint external_texture_id_;
	GLuint vao_;
	GLuint vbo_;
	GLuint shader_program_;
	float red_;
	float green_;
	float blue_;
	int x_;
	int y_;
	int width_;
	int height_;
	int last_frame_x_;
	int last_frame_y_;
	int last_frame_width_;
	int last_frame_height_;
	float texcoord_left_;
	float texcoord_top_;
	float texcoord_right_;
	float texcoord_bottom_;
	bool initialized_;
	bool has_valid_bounds_;
};
