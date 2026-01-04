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
			}
			else if (msg == WM_LBUTTONUP)
			{
				left_mouse_pressed_ = false;
			}
		}
	}
}
