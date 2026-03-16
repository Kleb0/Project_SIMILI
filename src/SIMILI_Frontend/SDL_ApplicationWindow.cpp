#include "SDL_ApplicationWindow.hpp"
#include "ui_handler.hpp"

SDL_ApplicationWindow::SDL_ApplicationWindow()
	: window_(nullptr)
	, is_maximized_(false)
	, last_x_(0)
	, last_y_(0)
	, last_width_(800)
	, last_height_(600)
	, dpi_scale_(1.0f)
{
}

SDL_ApplicationWindow::~SDL_ApplicationWindow()
{
	destroy();
}

bool SDL_ApplicationWindow::create(const std::string& title, int width, int height, Uint32 flags)
{
	if (window_)
	{
		std::cerr << "[SDL_ApplicationWindow] Window already exists" << std::endl;
		return false;
	}
	
	// Default flags if none specified
	if (flags == 0)
	{
		flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	}
	
	window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
	
	if (!window_)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create window: " << SDL_GetError() << std::endl;
		return false;
	}
	
	last_width_ = width;
	last_height_ = height;
	
	// Get initial position
	SDL_GetWindowPosition(window_, &last_x_, &last_y_);
	
	updateDpiScale();
	updateMaximizedState();
	
	std::cout << "[SDL_ApplicationWindow] Window created: " << title 
	          << " (" << width << "x" << height << ") DPI scale: " << dpi_scale_ << std::endl;
	
	return true;
}

void SDL_ApplicationWindow::destroy()
{
	if (window_)
	{
		SDL_DestroyWindow(window_);
		window_ = nullptr;
		std::cout << "[SDL_ApplicationWindow] Window destroyed" << std::endl;
	}
}

void SDL_ApplicationWindow::setPosition(int x, int y)
{
	if (window_)
	{
		SDL_SetWindowPosition(window_, x, y);
		last_x_ = x;
		last_y_ = y;
	}
}

void SDL_ApplicationWindow::setSize(int width, int height)
{
	if (window_)
	{
		SDL_SetWindowSize(window_, width, height);
		last_width_ = width;
		last_height_ = height;
	}
}

void SDL_ApplicationWindow::setTitle(const std::string& title)
{
	if (window_)
	{
		SDL_SetWindowTitle(window_, title.c_str());
	}
}

void SDL_ApplicationWindow::show()
{
	if (window_)
	{
		SDL_ShowWindow(window_);
	}
}

void SDL_ApplicationWindow::hide()
{
	if (window_)
	{
		SDL_HideWindow(window_);
	}
}

void SDL_ApplicationWindow::maximize()
{
	if (window_)
	{
		SDL_MaximizeWindow(window_);
		updateMaximizedState();
	}
}

void SDL_ApplicationWindow::restore()
{
	if (window_)
	{
		SDL_RestoreWindow(window_);
		updateMaximizedState();
	}
}

bool SDL_ApplicationWindow::isMaximized() const
{
	if (!window_)
		return false;
	
	Uint32 flags = SDL_GetWindowFlags(window_);
	return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

bool SDL_ApplicationWindow::isVisible() const
{
	if (!window_)
		return false;
	
	Uint32 flags = SDL_GetWindowFlags(window_);
	return !(flags & SDL_WINDOW_HIDDEN);
}

void SDL_ApplicationWindow::getPosition(int& x, int& y) const
{
	if (window_)
	{
		SDL_GetWindowPosition(window_, &x, &y);
	}
	else
	{
		x = last_x_;
		y = last_y_;
	}
}

void SDL_ApplicationWindow::getSize(int& width, int& height) const
{
	if (window_)
	{
		SDL_GetWindowSize(window_, &width, &height);
	}
	else
	{
		width = last_width_;
		height = last_height_;
	}
}

SDL_Rect SDL_ApplicationWindow::getBounds() const
{
	SDL_Rect bounds;
	getPosition(bounds.x, bounds.y);
	getSize(bounds.w, bounds.h);
	return bounds;
}

void SDL_ApplicationWindow::getBorderOffsets(int& left, int& top, int& right, int& bottom) const
{
	left = top = right = bottom = 0;
	
	if (!window_ || !isMaximized())
		return;
	
	SDL_Rect borderSize;
	if (SDL_GetWindowBordersSize(window_, &borderSize.y, &borderSize.x, &borderSize.h, &borderSize.w) == 0)
	{
		left = borderSize.x;
		top = borderSize.y;
		right = borderSize.w;
		bottom = borderSize.h;
	}
}

float SDL_ApplicationWindow::getDpiScale() const
{
	return dpi_scale_;
}

void SDL_ApplicationWindow::processEvents()
{
	if (!window_)
		return;
	
	// Check for position changes
	int currentX, currentY;
	SDL_GetWindowPosition(window_, &currentX, &currentY);
	
	if (currentX != last_x_ || currentY != last_y_)
	{
		last_x_ = currentX;
		last_y_ = currentY;
		std::cout << "[SDL_ApplicationWindow] Window moved to (" << currentX << ", " << currentY << ")" << std::endl;
		captureFrameData();
	}
	
	// Check for size changes
	int currentWidth, currentHeight;
	SDL_GetWindowSize(window_, &currentWidth, &currentHeight);
	
	if (currentWidth != last_width_ || currentHeight != last_height_)
	{
		last_width_ = currentWidth;
		last_height_ = currentHeight;
		std::cout << "[SDL_ApplicationWindow] Window resized to " << currentWidth << "x" << currentHeight << std::endl;
		captureFrameData();
	}
	
	// Check for maximized state changes
	bool currentMax = isMaximized();
	if (currentMax != is_maximized_)
	{
		is_maximized_ = currentMax;
		std::cout << "[SDL_ApplicationWindow] Window " << (is_maximized_ ? "MAXIMIZED" : "RESTORED") << std::endl;
		captureFrameData();
	}
}

void SDL_ApplicationWindow::updateDpiScale()
{
	if (!window_)
	{
		dpi_scale_ = 1.0f;
		return;
	}
	
	SDL_DisplayID displayID = SDL_GetDisplayForWindow(window_);
	if (displayID != 0)
	{
		dpi_scale_ = SDL_GetDisplayContentScale(displayID);
	}
	else
	{
		dpi_scale_ = 1.0f;
	}
}

void SDL_ApplicationWindow::updateMaximizedState()
{
	is_maximized_ = isMaximized();
}

void SDL_ApplicationWindow::Set_UIHandler(void* handler)
{
	ui_handler_ = handler;
}

void SDL_ApplicationWindow::captureFrameData()
{
	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		handler->captureIFramePositions();
	}
}
