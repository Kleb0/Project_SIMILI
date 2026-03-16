#pragma once

#include "MouseStates/Mouse_State.hpp"
#include "KeyManager.hpp"
#include <SDL3/SDL.h>
#include <windows.h>
#include <string>
#include <map>

namespace SIMILI
{
	namespace Input
	{
		struct MouseControlFrameData
		{
			std::string name;
			int clientX;
			int clientY;
			int width;
			int height;
			int marginLeft;
			int marginRight;
		};


		class Mouse_Above_Overlay_State;
		class Mouse_Outside_Overlay_State;
		class Mouse_Above_UI_Panel_State;

		class MouseController
		{
			public:
				struct ViewportBounds
				{
					int x;
					int y;
					int width;
					int height;
					int marginLeft;
					int marginRight;

					ViewportBounds() : x(0), y(0), width(0), height(0), marginLeft(0), marginRight(0) {}

					bool contains(int px, int py) const
					{
						return px >= (x - marginLeft) && px < (x + width + marginRight) && py >= y && py < (y + height);
					}
				};

			
				MouseController();
				~MouseController();

				struct PanelBounds
				{

					ViewportBounds viewport;
					ViewportBounds objectInspector;
					ViewportBounds history;
					ViewportBounds projectViewer;
					ViewportBounds panelAboveUI;
					ViewportBounds hierarchy;
				};



				void setMouseState(Mouse_State* state);
				void setWindowHandle(SDL_Window* sdlWindow);
				void setMaximizedState(bool isMaximized, int offsetX = 0, int offsetY = 0);
				void updateDpiScale();
				float getDpiScale() const { return dpi_scale_; }
				
				bool isShiftLeftClickActive();
				
				void processMouseInput(UINT msg, WPARAM wParam);
				void processMouseWheelInput(UINT msg, WPARAM wParam);
				void processMouseMove(int mouseX, int mouseY);
				void updateMousePosition();
				void updateMouseControlPanelBoundsFromFrameData(const std::map<std::string, MouseControlFrameData>& frameDataMap);
				std::string detectMouseRegionFromClientCoordinates(int clientX, int clientY) const;
				
				void transitionMouseState(const std::string& regionName, int mouseX, int mouseY);
				
				Mouse_State* getCurrentMouseState() const { return current_mouse_state_; }
				Mouse_Above_Overlay_State* getAboveOverlayState() const { return above_overlay_state_; }
				Mouse_Outside_Overlay_State* getOutsideOverlayState() const { return outside_overlay_state_; }
				Mouse_Above_UI_Panel_State* getAboveUIPanelState() const { return above_ui_panel_state_; }
				const std::string& getLastDetectedRegionName() const { return last_detected_region_name_; }
				void setLastDetectedRegionName(const std::string& regionName) { last_detected_region_name_ = regionName; }

				int getMouseDeltaX() const { return mouse_delta_x_; }
				int getMouseDeltaY() const { return mouse_delta_y_; }
				
				int getCurrentMouseX() const { return current_mouse_x_; }
				int getCurrentMouseY() const { return current_mouse_y_; }
				int getCurrentMouseClientX() const { return current_mouse_client_x_; }
				int getCurrentMouseClientY() const { return current_mouse_client_y_; }
				
				void resetMousePosition();
				
				bool isLeftButtonClicking() const { return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0; }
				bool hasClickEvent();
				bool isClickHeldForDuration(DWORD durationMs = 1000);
				
				// Mouse wheel scroll management
				int getMouseWheelDirection() const { return mouse_wheel_direction_; }
				void resetWheelDirection() { mouse_wheel_direction_ = 0; }
				bool hasWheelInput() const { return mouse_wheel_direction_ != 0; }
				void updateWheelState();

			private:
				Mouse_State* current_mouse_state_;
				Mouse_Above_Overlay_State* above_overlay_state_;
				Mouse_Outside_Overlay_State* outside_overlay_state_;
				Mouse_Above_UI_Panel_State* above_ui_panel_state_;
				std::string last_detected_region_name_;
				float dpi_scale_;
				bool is_maximized_;
				int maximized_offset_x_;
				int maximized_offset_y_;
				SDL_Window* window_handle_;
				bool left_mouse_pressed_;
				bool was_left_button_down_;
				
				KeyManager& key_manager_;
				
				int current_mouse_x_;
				int current_mouse_y_;
				int current_mouse_client_x_;
				int current_mouse_client_y_;
				int previous_mouse_x_;
				int previous_mouse_y_;
				int mouse_delta_x_;
				int mouse_delta_y_;
				bool mouse_position_initialized_;
				PanelBounds panel_bounds_;

				int additionnal_right_margin_offset_ = 0;
				
				float smoothed_delta_x_;
				float smoothed_delta_y_;
				const float smoothing_factor_ = 0.3f;
				
				int mouse_wheel_direction_;
				DWORD last_wheel_input_time_;
				const DWORD wheel_timeout_ms_ = 100;
				
				bool click_event_pending_;
				int click_start_x_;
				int click_start_y_;
				const int click_movement_threshold_ = 5;
				
				DWORD click_start_time_;
		};
	}
}
