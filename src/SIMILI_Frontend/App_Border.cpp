#include "App_Border.hpp"
#include <iostream>

App_Border::App_Border()
	: left_(BORDER_OFFSET)
	, right_(BORDER_OFFSET)
	, top_(BORDER_OFFSET)
	, bottom_(BORDER_OFFSET)
	, width_(0)
	, height_(0)
	, reference_window_width_(0)
	, reference_window_height_(0)
	, first_update_(true)
	, debug_line_enabled_(false)
	, current_state_(BorderState::Init)
{
}

App_Border::~App_Border()
{
}

void App_Border::updateDimensions(int windowWidth, int windowHeight)
{
	if (first_update_)
	{
		left_ = BORDER_OFFSET;
		top_ = BORDER_OFFSET;
		right_ = windowWidth - BORDER_OFFSET;
		bottom_ = windowHeight - BORDER_OFFSET;
		width_ = right_ - left_;
		height_ = bottom_ - top_;
		reference_window_width_ = windowWidth;
		reference_window_height_ = windowHeight;
		
		std::cout << "[App_Border] Initial dimensions: " << "left=" << left_ << " top=" << top_ 
			<< " right=" << right_ << " bottom=" << bottom_ << " width=" << width_ << " height=" << height_ 
			<< " ref_window=" << reference_window_width_ << "x" << reference_window_height_ << std::endl;
		
		first_update_ = false;
		current_state_ = BorderState::Init;
		return;
	}

	if (windowWidth > reference_window_width_ || windowHeight > reference_window_height_)
	{
		if (current_state_ != BorderState::Maximized)
		{

			current_state_ = BorderState::Maximized;

			// translate in english 

			std::cout << "[App_Border] Switching to MAXIMIZED state (window: " << windowWidth << "x" << windowHeight 
				<< " vs reference: " << reference_window_width_ << "x" << reference_window_height_ << ")" << std::endl;
		}
		return;
	}

	if (windowWidth == reference_window_width_ && windowHeight == reference_window_height_)
	{
		if (current_state_ != BorderState::Init)
		{
			std::cout << "[App_Border] Returning to INIT state (reference dimensions)" << std::endl;
			current_state_ = BorderState::Init;
		}
		return;
	}

	if (windowWidth < reference_window_width_ || windowHeight < reference_window_height_)
	{
		left_ = BORDER_OFFSET;
		top_ = BORDER_OFFSET;
		right_ = windowWidth - BORDER_OFFSET;
		bottom_ = windowHeight - BORDER_OFFSET;
		width_ = right_ - left_;
		height_ = bottom_ - top_;

		if (current_state_ != BorderState::Reduced)
		{
			std::cout << "[App_Border] Switching to REDUCED state (window: " << windowWidth << "x" << windowHeight 
				<< " vs reference: " << reference_window_width_ << "x" << reference_window_height_ << ")" << std::endl;
			current_state_ = BorderState::Reduced;
		}
		return;
	}
}

int App_Border::getLeft() const
{
	return left_;
}

int App_Border::getRight() const
{
	return right_;
}

int App_Border::getTop() const
{
	return top_;
}

int App_Border::getBottom() const
{
	return bottom_;
}

int App_Border::getWidth() const
{
	return width_;
}

int App_Border::getHeight() const
{
	return height_;
}

void App_Border::enableDebugLine(bool enabled)
{
	debug_line_enabled_ = enabled;
}

bool App_Border::isDebugLineEnabled() const
{
	return debug_line_enabled_;
}

BorderState App_Border::getCurrentState() const
{
	return current_state_;
}

void App_Border::setState(BorderState newState)
{
	current_state_ = newState;
}

int App_Border::getReferenceWindowWidth() const
{
	return reference_window_width_;
}

int App_Border::getReferenceWindowHeight() const
{
	return reference_window_height_;
}
