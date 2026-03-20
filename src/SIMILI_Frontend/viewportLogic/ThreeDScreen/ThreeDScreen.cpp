#include "pch.hpp"
#include "ThreeDScreen.hpp"
#include "../FrameDatas/FrameDatas.hpp"
#include <cmath>
#include <iostream>

ThreeDScreen::ThreeDScreen()
	: x_(0)
	, y_(0)
	, width_(0)
	, height_(0)
	, initialized_(false)
	, has_valid_viewport_(false)
	, waiting_for_frame_data_logged_(false)
	, first_render_logged_(false)
	, camera_(nullptr)
{
}

ThreeDScreen::~ThreeDScreen()
{
}

void ThreeDScreen::initialize()
{
	initialized_ = true;
}

void ThreeDScreen::render(SIMILI::Frontend::FrameDatas* frameDatas, SDL_Window* window)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	if (!initialized_)
	{
		std::cout << "[ThreeDScreen] Not initialized yet" << std::endl;
		return;
	}
	
	has_valid_viewport_ = false;

	if (!frameDatas || !window)
	{
		if (!waiting_for_frame_data_logged_)
		{
			std::cout << "[ThreeDScreen] frameDataMap is EMPTY - waiting for JavaScript data" << std::endl;
			waiting_for_frame_data_logged_ = true;
		}
		return;
	}
	
	SIMILI::Frontend::IFrameScreenData frameData;
	if (!frameDatas->getFrameData("viewport_panel", frameData))
	{
		if (!waiting_for_frame_data_logged_)
		{
			std::cout << "[ThreeDScreen] frameDataMap is EMPTY - waiting for JavaScript data" << std::endl;
			waiting_for_frame_data_logged_ = true;
		}
		return;
	}

	waiting_for_frame_data_logged_ = false;

	int logicalWindowWidth = 0;
	int logicalWindowHeight = 0;
	int drawableWidth = 0;
	int drawableHeight = 0;
	SDL_GetWindowSize(window, &logicalWindowWidth, &logicalWindowHeight);
	SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);

	if (logicalWindowWidth <= 0 || logicalWindowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0)
	{
		return;
	}
	
	float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(logicalWindowWidth);
	float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(logicalWindowHeight);

	int logicalX = frameData.relativeX + 5;
	int logicalY = frameData.relativeY + 5;
	int logicalWidth = frameData.width - 20;
	int logicalHeight = frameData.height - 20;
	
	if (logicalWidth <= 0 || logicalHeight <= 0)
	{
		std::cout << "[ThreeDScreen] Invalid dimensions: width=" << logicalWidth << " height=" << logicalHeight << std::endl;
		return;
	}
	
	x_ = static_cast<int>(std::lround(static_cast<float>(logicalX) * scaleX));
	width_ = static_cast<int>(std::lround(static_cast<float>(logicalWidth) * scaleX));
	height_ = static_cast<int>(std::lround(static_cast<float>(logicalHeight) * scaleY));
	int topY = static_cast<int>(std::lround(static_cast<float>(logicalY) * scaleY));
	y_ = drawableHeight - topY - height_;

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

	has_valid_viewport_ = width_ > 0 && height_ > 0;

	if (!has_valid_viewport_)
	{
		std::cout << "[ThreeDScreen] Invalid dimensions: width=" << width_ << " height=" << height_ << std::endl;
		return;
	}

	if (!first_render_logged_)
	{
		std::cout << "[ThreeDScreen] FIRST RENDER - viewport_panel found!" << std::endl;
		std::cout << "[ThreeDScreen] Position: x=" << x_ << " y=" << y_
		          << " width=" << width_ << " height=" << height_ << std::endl;
		first_render_logged_ = true;
	}
	}

void ThreeDScreen::draw()
	{
		std::lock_guard<std::mutex> lock(render_mutex_);

		if (!initialized_ || !has_valid_viewport_)
		{
			return;
		}
	}
