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

		private:
			Mouse_State* current_mouse_state_;
			bool left_mouse_pressed_;
			
			KeyManager& key_manager_;
		};
	}
}
