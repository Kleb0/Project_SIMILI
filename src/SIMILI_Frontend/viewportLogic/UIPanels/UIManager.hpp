#pragma once

#include <SDL3/SDL.h>

namespace SIMILI {
	namespace Frontend {

		enum class UIState
		{
			Normal,
			Maximized,
			Minimized,
			Resizing,
			Moving
		};

		class UIManager
		{
		public:
			UIManager();
			~UIManager();

			void updateUIState(SDL_Window* window);

			UIState getCurrentState() const { return current_state_; }
			bool hasStateChanged() const { return state_changed_; }
			void acknowledgeStateChange() { state_changed_ = false; }

			int getWindowWidth() const { return window_width_; }
			int getWindowHeight() const { return window_height_; }
			int getWindowX() const { return window_x_; }
			int getWindowY() const { return window_y_; }
			bool isWindowMaximized() const { return is_maximized_; }

		private:
			UIState current_state_;
			UIState previous_state_;
			bool state_changed_;

			int window_width_;
			int window_height_;
			int window_x_;
			int window_y_;
			bool is_maximized_;

			int previous_width_;
			int previous_height_;
			int previous_x_;
			int previous_y_;
			bool previous_maximized_;
		};
	}
}
