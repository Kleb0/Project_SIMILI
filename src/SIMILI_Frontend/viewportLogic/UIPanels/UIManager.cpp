#include "UIManager.hpp"

namespace SIMILI {
	namespace Frontend {

		UIManager::UIManager()
			: current_state_(UIState::Normal)
			, previous_state_(UIState::Normal)
			, state_changed_(false)
			, window_width_(0)
			, window_height_(0)
			, window_x_(0)
			, window_y_(0)
			, is_maximized_(false)
			, previous_width_(0)
			, previous_height_(0)
			, previous_x_(0)
			, previous_y_(0)
			, previous_maximized_(false)
		{

		}

		UIManager::~UIManager()
		{
		}

		void UIManager::updateUIState(SDL_Window* window)
		{
			if (!window)
			{
				return;
			}

			previous_width_ = window_width_;
			previous_height_ = window_height_;
			previous_x_ = window_x_;
			previous_y_ = window_y_;
			previous_maximized_ = is_maximized_;
			previous_state_ = current_state_;

			SDL_GetWindowSize(window, &window_width_, &window_height_);
			SDL_GetWindowPosition(window, &window_x_, &window_y_);

			Uint32 flags = SDL_GetWindowFlags(window);
			is_maximized_ = (flags & SDL_WINDOW_MAXIMIZED) != 0;
			bool is_minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;

			if (is_minimized)
			{
				current_state_ = UIState::Minimized;
			}
			else if (is_maximized_)
			{
				current_state_ = UIState::Maximized;
			}
			else if (previous_width_ != window_width_ || previous_height_ != window_height_)
			{
				current_state_ = UIState::Resizing;
			}
			else if (previous_x_ != window_x_ || previous_y_ != window_y_)
			{
				current_state_ = UIState::Moving;
			}
			else
			{
				current_state_ = UIState::Normal;
			}

			state_changed_ = (current_state_ != previous_state_);

        }
    }
}