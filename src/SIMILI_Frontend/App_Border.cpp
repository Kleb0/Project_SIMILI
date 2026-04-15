#include "App_Border.hpp"
#include <iostream>

App_Border::App_Border()
	: left_(BORDER_OFFSET)
	, right_(BORDER_OFFSET)
	, top_(BORDER_OFFSET)
	, bottom_(BORDER_OFFSET)
	, width_(0)
	, height_(0)
	, first_update_(true)
	, debug_line_enabled_(false)
{
}

App_Border::~App_Border()
{
}

void App_Border::updateDimensions(int windowWidth, int windowHeight)
{
	left_ = BORDER_OFFSET;
	top_ = BORDER_OFFSET;
	right_ = windowWidth - BORDER_OFFSET;
	bottom_ = windowHeight - BORDER_OFFSET;
	width_ = right_ - left_;
	height_ = bottom_ - top_;
	
	if (first_update_)
	{
		std::cout << "[App_Border] Dimensions initiales: " << "left=" << left_ << " top=" << top_ 
        << " right=" << right_ << " bottom=" << bottom_ << " width=" << width_ << " height=" << height_ << std::endl;
		first_update_ = false;
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
