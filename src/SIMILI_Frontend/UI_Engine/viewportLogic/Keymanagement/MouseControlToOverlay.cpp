#include "MouseControlToOverlay.hpp"
#include "MouseStates/Mouse_Above_Overlay_State.hpp"
#include <iostream>

namespace SIMILI
{
	namespace Input
	{
		MouseControlToOverlay::MouseControlToOverlay()
			: current_mouse_state_(nullptr)
			, left_mouse_pressed_(false)
			, was_left_button_down_(false)
			, key_manager_(KeyManager::getInstance())
			, current_mouse_x_(0)
			, current_mouse_y_(0)
			, previous_mouse_x_(0)
			, previous_mouse_y_(0)
			, mouse_delta_x_(0)
			, mouse_delta_y_(0)
			, mouse_position_initialized_(false)
			, smoothed_delta_x_(0.0f)
			, smoothed_delta_y_(0.0f)
			, mouse_wheel_direction_(0)
			, last_wheel_input_time_(0)
			, click_event_pending_(false)
			, click_start_x_(0)
			, click_start_y_(0)
			, click_start_time_(0)
		{
		}

		void MouseControlToOverlay::setMouseState(Mouse_State* state)
		{
			current_mouse_state_ = state;
		}

		bool MouseControlToOverlay::isShiftLeftClickActive()
		{
			if (!current_mouse_state_ || !current_mouse_state_->isActive())
			{
				return false;
			}

			Mouse_Above_Overlay_State* above_state = dynamic_cast<Mouse_Above_Overlay_State*>(current_mouse_state_);
			if (!above_state)
			{
				return false;
			}

			bool shift_pressed = key_manager_.getInputSystem()->isKeyPressed(VK_SHIFT);
			
			bool left_button_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			
			return shift_pressed && (left_mouse_pressed_ || left_button_down);
		}

		void MouseControlToOverlay::processMouseInput(UINT msg, WPARAM wParam)
		{
			if (msg == WM_LBUTTONDOWN)
			{
				left_mouse_pressed_ = true;
				click_start_x_ = current_mouse_x_;
				click_start_y_ = current_mouse_y_;
				click_start_time_ = GetTickCount();
			}
			else if (msg == WM_LBUTTONUP)
			{
				left_mouse_pressed_ = false;
				
				DWORD click_duration = GetTickCount() - click_start_time_;
				int dx = abs(current_mouse_x_ - click_start_x_);
				int dy = abs(current_mouse_y_ - click_start_y_);
				
				if (dx <= click_movement_threshold_ && dy <= click_movement_threshold_ && click_duration < 1000)
				{
					click_event_pending_ = true;
				}
			}
		}
		
		void MouseControlToOverlay::processMouseWheelInput(UINT msg, WPARAM wParam)
		{
			if (msg == WM_MOUSEWHEEL)
			{
				int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				last_wheel_input_time_ = GetTickCount();
				
				std::cout << "[MouseControlToOverlay] WM_MOUSEWHEEL detected! Delta: " << delta << std::endl;
				
				if (delta > 0)
				{
					mouse_wheel_direction_ = 1;
					std::cout << "[MouseControlToOverlay] Scroll UP (1)" << std::endl;
				}
				else if (delta < 0)
				{
					mouse_wheel_direction_ = -1;
					std::cout << "[MouseControlToOverlay] Scroll DOWN (-1)" << std::endl;
				}
				else
				{
					mouse_wheel_direction_ = 0;
				}
			}
		}
		
		void MouseControlToOverlay::updateWheelState()
		{
			// Reset wheel direction if timeout has elapsed since last input
			if (mouse_wheel_direction_ != 0)
			{
				DWORD current_time = GetTickCount();
				if (current_time - last_wheel_input_time_ > wheel_timeout_ms_)
				{
					mouse_wheel_direction_ = 0;
				}
			}
		}
		
		void MouseControlToOverlay::processMouseMove(int mouseX, int mouseY)
		{
			if (!mouse_position_initialized_)
			{
				current_mouse_x_ = mouseX;
				current_mouse_y_ = mouseY;
				previous_mouse_x_ = mouseX;
				previous_mouse_y_ = mouseY;
				mouse_delta_x_ = 0;
				mouse_delta_y_ = 0;
				smoothed_delta_x_ = 0.0f;
				smoothed_delta_y_ = 0.0f;
				mouse_position_initialized_ = true;
				return;
			}
			
			previous_mouse_x_ = current_mouse_x_;
			previous_mouse_y_ = current_mouse_y_;
			
			current_mouse_x_ = mouseX;
			current_mouse_y_ = mouseY;
			
			int raw_delta_x = current_mouse_x_ - previous_mouse_x_;
			int raw_delta_y = current_mouse_y_ - previous_mouse_y_;
			
			smoothed_delta_x_ = smoothed_delta_x_ * (1.0f - smoothing_factor_) + raw_delta_x * smoothing_factor_;
			smoothed_delta_y_ = smoothed_delta_y_ * (1.0f - smoothing_factor_) + raw_delta_y * smoothing_factor_;
			
			mouse_delta_x_ = static_cast<int>(smoothed_delta_x_);
			mouse_delta_y_ = static_cast<int>(smoothed_delta_y_);
		}
		
		void MouseControlToOverlay::updateMousePosition()
		{
			POINT cursorPos;
			if (GetCursorPos(&cursorPos))
			{
				processMouseMove(cursorPos.x, cursorPos.y);
			}
			
			bool is_left_button_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			
			if (is_left_button_down && !was_left_button_down_)
			{
				left_mouse_pressed_ = true;
				click_start_x_ = current_mouse_x_;
				click_start_y_ = current_mouse_y_;
				click_start_time_ = GetTickCount();
			}
			else if (!is_left_button_down && was_left_button_down_)
			{
				left_mouse_pressed_ = false;
				
				DWORD click_duration = GetTickCount() - click_start_time_;
				int dx = abs(current_mouse_x_ - click_start_x_);
				int dy = abs(current_mouse_y_ - click_start_y_);
				
				if (dx <= click_movement_threshold_ && dy <= click_movement_threshold_ && click_duration < 200)
				{
					click_event_pending_ = true;
				}
			}
			
			was_left_button_down_ = is_left_button_down;
		}
		
		void MouseControlToOverlay::resetMousePosition()
		{
			mouse_position_initialized_ = false;
			mouse_delta_x_ = 0;
			mouse_delta_y_ = 0;
			smoothed_delta_x_ = 0.0f;
			smoothed_delta_y_ = 0.0f;
		}
		
		bool MouseControlToOverlay::hasClickEvent()
		{
			if (click_event_pending_)
			{
				click_event_pending_ = false;
				return true;
			}
			return false;
		}
		
		bool MouseControlToOverlay::isClickHeldForDuration(DWORD durationMs)
		{
			if (!left_mouse_pressed_)
			{
				return false;
			}
			
			DWORD elapsed_time = GetTickCount() - click_start_time_;
			return elapsed_time >= durationMs;
		}
	}
}
