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
			void processMouseMove(int mouseX, int mouseY);
			void updateMousePosition();

			int getMouseDeltaX() const { return mouse_delta_x_; }
			int getMouseDeltaY() const { return mouse_delta_y_; }
			
			void resetMousePosition();

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
		};
	}
}
