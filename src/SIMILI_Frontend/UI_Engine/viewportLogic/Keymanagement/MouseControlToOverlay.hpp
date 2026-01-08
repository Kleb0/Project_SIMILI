#pragma once

#include "MouseStates/Mouse_State.hpp"
#include "KeyManager.hpp"
#include <windows.h>

namespace SIMILI
{
	namespace Input
	{
		class MouseControlToOverlay
		{
			public:
				MouseControlToOverlay();
				~MouseControlToOverlay() = default;

				void setMouseState(Mouse_State* state);
				
				bool isShiftLeftClickActive();
				
				void processMouseInput(UINT msg, WPARAM wParam);
				void processMouseWheelInput(UINT msg, WPARAM wParam);
				void processMouseMove(int mouseX, int mouseY);
				void updateMousePosition();

				int getMouseDeltaX() const { return mouse_delta_x_; }
				int getMouseDeltaY() const { return mouse_delta_y_; }
				
				void resetMousePosition();
				
				bool isLeftButtonClicking() const { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }
				
				// Mouse wheel scroll management
				int getMouseWheelDirection() const { return mouse_wheel_direction_; }
				void resetWheelDirection() { mouse_wheel_direction_ = 0; }
				bool hasWheelInput() const { return mouse_wheel_direction_ != 0; }
				void updateWheelState();

			private:
				Mouse_State* current_mouse_state_;
				bool left_mouse_pressed_;
				
				KeyManager& key_manager_;
				
				int current_mouse_x_;
				int current_mouse_y_;
				int previous_mouse_x_;
				int previous_mouse_y_;
				int mouse_delta_x_;
				int mouse_delta_y_;
				bool mouse_position_initialized_;
				
				float smoothed_delta_x_;
				float smoothed_delta_y_;
				const float smoothing_factor_ = 0.3f;
				
				int mouse_wheel_direction_;
				DWORD last_wheel_input_time_;
				const DWORD wheel_timeout_ms_ = 100;
		};
	}
}
