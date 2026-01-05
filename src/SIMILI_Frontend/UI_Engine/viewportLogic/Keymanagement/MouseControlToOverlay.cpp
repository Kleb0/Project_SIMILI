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
			, key_manager_(KeyManager::getInstance())
			, current_mouse_x_(0)
			, current_mouse_y_(0)
			, previous_mouse_x_(0)
			, previous_mouse_y_(0)
			, mouse_delta_x_(0)
			, mouse_delta_y_(0)
			, mouse_position_initialized_(false)
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
			
			// std::cout << "[MouseControlToOverlay] CHECK: shift=" << shift_pressed 
			//           << " left_mouse_pressed_=" << left_mouse_pressed_
			//           << " GetAsyncKeyState(LBUTTON)=" << left_button_down << std::endl;
			
			return shift_pressed && (left_mouse_pressed_ || left_button_down);
		}

		void MouseControlToOverlay::processMouseInput(UINT msg, WPARAM wParam)
		{
			if (msg == WM_LBUTTONDOWN)
			{
				left_mouse_pressed_ = true;
			}
			else if (msg == WM_LBUTTONUP)
			{
				left_mouse_pressed_ = false;
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
				mouse_position_initialized_ = true;
				return;
			}
			
			previous_mouse_x_ = current_mouse_x_;
			previous_mouse_y_ = current_mouse_y_;
			
			current_mouse_x_ = mouseX;
			current_mouse_y_ = mouseY;
			
			mouse_delta_x_ = current_mouse_x_ - previous_mouse_x_;
			mouse_delta_y_ = current_mouse_y_ - previous_mouse_y_;
		}
		
		void MouseControlToOverlay::updateMousePosition()
		{
			POINT cursorPos;
			if (GetCursorPos(&cursorPos))
			{
				processMouseMove(cursorPos.x, cursorPos.y);
			}
		}
		
		void MouseControlToOverlay::resetMousePosition()
		{
			mouse_position_initialized_ = false;
			mouse_delta_x_ = 0;
			mouse_delta_y_ = 0;
		}
	}
}
