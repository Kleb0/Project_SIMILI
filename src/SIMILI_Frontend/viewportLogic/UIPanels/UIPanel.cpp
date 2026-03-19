#define GLM_ENABLE_EXPERIMENTAL

#include "UIPanel.hpp"
#include "../../CEF_Drawer.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

static const char* uiPanelVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
	gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
	TexCoord = aTexCoord;
}
)";

static const char* uiPanelFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D panelTexture;

void main()
{
	FragColor = texture(panelTexture, TexCoord);
}
)";

UIPanel::UIPanel()
	: texture_id_(0)
	, external_texture_id_(0)
	, vao_(0)
	, vbo_(0)
	, shader_program_(0)
	, red_(0.0f)
	, green_(0.0f)
	, blue_(0.0f)
	, x_(0)
	, y_(0)
	, width_(0)
	, height_(0)
	, last_frame_x_(0)
	, last_frame_y_(0)
	, last_frame_width_(0)
	, last_frame_height_(0)
	, texcoord_left_(0.0f)
	, texcoord_top_(0.0f)
	, texcoord_right_(1.0f)
	, texcoord_bottom_(1.0f)
	, initialized_(false)
	, has_valid_bounds_(false)
{
}

UIPanel::~UIPanel()
{
	shutdown();
}

bool UIPanel::initialize(const std::string& panelName)
{
	if (initialized_)
	{
		return true;
	}

	name_ = panelName;

	std::size_t hashValue = std::hash<std::string>{}(name_);
	red_ = 0.25f + (static_cast<float>(hashValue & 0xFFu) / 255.0f) * 0.55f;
	green_ = 0.25f + (static_cast<float>((hashValue >> 8) & 0xFFu) / 255.0f) * 0.55f;
	blue_ = 0.25f + (static_cast<float>((hashValue >> 16) & 0xFFu) / 255.0f) * 0.55f;

	if (!createTexture())
	{
		return false;
	}

	if (!createShaderProgram())
	{
		shutdown();
		return false;
	}

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);

	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	initialized_ = true;
	return true;
}

void UIPanel::shutdown()
{
	if (vbo_)
	{
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}

	if (vao_)
	{
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}

	if (texture_id_)
	{
		glDeleteTextures(1, &texture_id_);
		texture_id_ = 0;
	}

	if (shader_program_)
	{
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
	}

	initialized_ = false;
	has_valid_bounds_ = false;
	external_texture_id_ = 0;
	last_frame_x_ = 0;
	last_frame_y_ = 0;
	last_frame_width_ = 0;
	last_frame_height_ = 0;
	texcoord_left_ = 0.0f;
	texcoord_top_ = 0.0f;
	texcoord_right_ = 1.0f;
	texcoord_bottom_ = 1.0f;
}

void UIPanel::updateFromFrameData(const SIMILI::Frontend::IFrameScreenData& frameData, SDL_Window* window, bool skipTextureRebuild)
{
	if (!window)
	{
		has_valid_bounds_ = false;
		return;
	}

	int logicalWindowWidth = 0;
	int logicalWindowHeight = 0;
	int drawableWidth = 0;
	int drawableHeight = 0;
	SDL_GetWindowSize(window, &logicalWindowWidth, &logicalWindowHeight);
	SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);

	if (logicalWindowWidth <= 0 || logicalWindowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0)
	{
		has_valid_bounds_ = false;
		return;
	}

	float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(logicalWindowWidth);
	float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(logicalWindowHeight);

	x_ = static_cast<int>(std::lround(static_cast<float>(frameData.relativeX) * scaleX));
	y_ = static_cast<int>(std::lround(static_cast<float>(frameData.relativeY) * scaleY));
	width_ = static_cast<int>(std::lround(static_cast<float>(frameData.width) * scaleX));
	height_ = static_cast<int>(std::lround(static_cast<float>(frameData.height) * scaleY));

	if (x_ < 0)
	{
		width_ += x_;
		x_ = 0;
	}

	if (y_ < 0)
	{
		height_ += y_;
		y_ = 0;
	}

	if (x_ + width_ > drawableWidth)
	{
		width_ = drawableWidth - x_;
	}

	if (y_ + height_ > drawableHeight)
	{
		height_ = drawableHeight - y_;
	}

	has_valid_bounds_ = width_ > 0 && height_ > 0;

	// Update display frame with SDL drawable coordinates (with DPI scaling)
	CEF_Drawer::UIPanelFrameData displayFrame;
	displayFrame.x = x_;
	displayFrame.y = y_;
	displayFrame.width = width_;
	displayFrame.height = height_;
	CEF_Drawer::updateActiveUIPanelDisplayFrame(name_, displayFrame);

	// Update source frame with CEF logical coordinates (without DPI scaling)
	CEF_Drawer::UIPanelFrameData sourceFrame;
	sourceFrame.x = frameData.relativeX;
	sourceFrame.y = frameData.relativeY;
	sourceFrame.width = frameData.width;
	sourceFrame.height = frameData.height;
	CEF_Drawer::updateActiveUIPanelSourceFrame(name_, sourceFrame);

	const bool frameGeometryChanged =
		frameData.relativeX != last_frame_x_ || frameData.relativeY != last_frame_y_ ||
		frameData.width != last_frame_width_ || frameData.height != last_frame_height_;

	if (frameGeometryChanged)
	{
		last_frame_x_ = frameData.relativeX;
		last_frame_y_ = frameData.relativeY;
		last_frame_width_ = frameData.width;
		last_frame_height_ = frameData.height;
	}

	if (!skipTextureRebuild && (frameGeometryChanged || CEF_Drawer::isActiveUIPanelTextureDirty(name_)))
	{
		updateTextureRegion();
	}
}

void UIPanel::draw(int drawableWidth, int drawableHeight)
{
	if (!initialized_ || !has_valid_bounds_ || drawableWidth <= 0 || drawableHeight <= 0)
	{
		return;
	}

	updateGeometry(drawableWidth, drawableHeight);

	glUseProgram(shader_program_);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, external_texture_id_ != 0 ? external_texture_id_ : texture_id_);
	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void UIPanel::updateTextureRegion()
{
	external_texture_id_ = 0;
	texcoord_left_ = 0.0f;
	texcoord_top_ = 0.0f;
	texcoord_right_ = 1.0f;
	texcoord_bottom_ = 1.0f;

	GLuint textureId = 0;
	int textureWidth = 0;
	int textureHeight = 0;
	CEF_Drawer::UIPanelFrameData panelFrame = {};

	if (!CEF_Drawer::getActiveUIPanelTextureRegion(name_, textureId, textureWidth, textureHeight, panelFrame))
	{
		return;
	}

	if (textureWidth <= 0 || textureHeight <= 0)
	{
		return;
	}

	float left = static_cast<float>(panelFrame.x) / static_cast<float>(textureWidth);
	float top = static_cast<float>(panelFrame.y) / static_cast<float>(textureHeight);
	float right = static_cast<float>(panelFrame.x + panelFrame.width) / static_cast<float>(textureWidth);
	float bottom = static_cast<float>(panelFrame.y + panelFrame.height) / static_cast<float>(textureHeight);

	left = std::clamp(left, 0.0f, 1.0f);
	top = std::clamp(top, 0.0f, 1.0f);
	right = std::clamp(right, 0.0f, 1.0f);
	bottom = std::clamp(bottom, 0.0f, 1.0f);

	if (right <= left || bottom <= top)
	{
		return;
	}

	external_texture_id_ = textureId;
	texcoord_left_ = left;
	texcoord_top_ = top;
	texcoord_right_ = right;
	texcoord_bottom_ = bottom;
}

bool UIPanel::createTexture()
{
	unsigned char pixel[4] = {
		static_cast<unsigned char>(std::lround(red_ * 255.0f)),
		static_cast<unsigned char>(std::lround(green_ * 255.0f)),
		static_cast<unsigned char>(std::lround(blue_ * 255.0f)),
		255
	};

	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

	return texture_id_ != 0;
}

bool UIPanel::createShaderProgram()
{
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &uiPanelVertexShaderSource, nullptr);
	glCompileShader(vertexShader);

	GLint success = 0;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512] = { 0 };
		glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "[UIPanel] Vertex shader compilation failed: " << infoLog << std::endl;
		glDeleteShader(vertexShader);
		return false;
	}

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &uiPanelFragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512] = { 0 };
		glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "[UIPanel] Fragment shader compilation failed: " << infoLog << std::endl;
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return false;
	}

	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertexShader);
	glAttachShader(shader_program_, fragmentShader);
	glLinkProgram(shader_program_);
	glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	if (!success)
	{
		char infoLog[512] = { 0 };
		glGetProgramInfoLog(shader_program_, sizeof(infoLog), nullptr, infoLog);
		std::cerr << "[UIPanel] Shader program linking failed: " << infoLog << std::endl;
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
		return false;
	}

	glUseProgram(shader_program_);
	glUniform1i(glGetUniformLocation(shader_program_, "panelTexture"), 0);

	return true;
}

void UIPanel::updateGeometry(int drawableWidth, int drawableHeight)
{
	float left = (static_cast<float>(x_) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	float right = (static_cast<float>(x_ + width_) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	float top = 1.0f - (static_cast<float>(y_) / static_cast<float>(drawableHeight)) * 2.0f;
	float bottom = 1.0f - (static_cast<float>(y_ + height_) / static_cast<float>(drawableHeight)) * 2.0f;

	float vertices[] = {
		left, top, texcoord_left_, texcoord_top_,
		right, bottom, texcoord_right_, texcoord_bottom_,
		left, bottom, texcoord_left_, texcoord_bottom_,
		left, top, texcoord_left_, texcoord_top_,
		right, top, texcoord_right_, texcoord_top_,
		right, bottom, texcoord_right_, texcoord_bottom_
	};

	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void UIPanel::forceTextureRebuild()
{
	// Reset last frame dimensions to force a rebuild on next update
	last_frame_x_ = -1;
	last_frame_y_ = -1;
	last_frame_width_ = -1;
	last_frame_height_ = -1;
}
