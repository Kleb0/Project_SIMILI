#include "MouseController.hpp"
#include "MouseStates/Mouse_Above_Overlay_State.hpp"
#include "MouseStates/Mouse_Outside_Overlay_State.hpp"
#include "MouseStates/Mouse_Above_UI_Panel_State.hpp"
#include <iostream>
#include <mutex>
#include <cmath>

static std::mutex g_panel_bounds_mutex;

namespace SIMILI
{
	namespace Input
	{
		MouseController::MouseController()
			: current_mouse_state_(nullptr)
			, above_overlay_state_(nullptr)
			, outside_overlay_state_(nullptr)
			, above_ui_panel_state_(nullptr)
			, last_detected_region_name_("")
			, dpi_scale_(1.0f)
			, is_maximized_(false)
			, maximized_offset_x_(0)
			, maximized_offset_y_(0)
			, window_handle_(nullptr)
			, left_mouse_pressed_(false)
			, was_left_button_down_(false)
			, key_manager_(KeyManager::getInstance())
			, current_mouse_x_(0)
			, current_mouse_y_(0)
			, current_mouse_client_x_(0)
			, current_mouse_client_y_(0)
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
			above_overlay_state_ = new Mouse_Above_Overlay_State();
			outside_overlay_state_ = new Mouse_Outside_Overlay_State();
			above_ui_panel_state_ = new Mouse_Above_UI_Panel_State();

			current_mouse_state_ = outside_overlay_state_;
			if (current_mouse_state_)
			{
				current_mouse_state_->onEnter();
			}
		}

		MouseController::~MouseController()
		{
			if (above_overlay_state_)
			{
				delete above_overlay_state_;
				above_overlay_state_ = nullptr;
			}
			if (outside_overlay_state_)
			{
				delete outside_overlay_state_;
				outside_overlay_state_ = nullptr;
			}
			if (above_ui_panel_state_)
			{
				delete above_ui_panel_state_;
				above_ui_panel_state_ = nullptr;
			}
			current_mouse_state_ = nullptr;
		}

		void MouseController::setMouseState(Mouse_State* state)
		{
			current_mouse_state_ = state;
		}

		void MouseController::setWindowHandle(SDL_Window* sdlWindow)
		{
			window_handle_ = sdlWindow;
		}

		void MouseController::setMaximizedState(bool isMaximized, int offsetX, int offsetY)
		{
			is_maximized_ = isMaximized;
			maximized_offset_x_ = offsetX;
			maximized_offset_y_ = offsetY;
			updateDpiScale();

			if (isMaximized)
			{
				std::cout << "[MouseController] Window maximized DPI Scale is " << dpi_scale_ << std::endl;
			}
			else
			{
				std::cout << "[MouseController] Window not maximized - no offsets" << std::endl;
			}
		}

		void MouseController::updateDpiScale()
		{
			if (!window_handle_)
			{
				dpi_scale_ = 1.0f;
				return;
			}

			int displayIndex = SDL_GetDisplayForWindow(window_handle_);
			if (displayIndex >= 0)
			{
				float ddpi = SDL_GetDisplayContentScale(displayIndex);
				if (ddpi > 0.0f)
				{
					dpi_scale_ = ddpi;
				}
				else
				{
					dpi_scale_ = 1.0f;
				}
			}
			else
			{
				dpi_scale_ = 1.0f;
			}
		}

		void MouseController::updateMouseControlPanelBoundsFromFrameData(const std::map<std::string, MouseControlFrameData>& frameDataMap)
		{
			std::lock_guard<std::mutex> lock(g_panel_bounds_mutex);

			for (const auto& pair : frameDataMap)
			{
				const MouseControlFrameData& data = pair.second;

				if (data.name == "hierarchy_panel")
				{
					panel_bounds_.hierarchy.x = data.clientX;
					panel_bounds_.hierarchy.y = data.clientY;
					panel_bounds_.hierarchy.width = data.width;
					panel_bounds_.hierarchy.height = data.height;
				}
				else if (data.name == "viewport_panel")
				{
					panel_bounds_.viewport.x = data.clientX;
					panel_bounds_.viewport.y = data.clientY;
					panel_bounds_.viewport.width = data.width;
					panel_bounds_.viewport.height = data.height;
					panel_bounds_.viewport.marginLeft = data.marginLeft;
					
					if (dpi_scale_ > 1.0f)
					{
						additionnal_right_margin_offset_ = +160;
						panel_bounds_.viewport.marginRight = data.marginRight + additionnal_right_margin_offset_;
						std::cout << "[MouseController] TEST TEST TES ! High DPI detected (scale: " << dpi_scale_ << ") - applying additional margin to viewport panel" << std::endl;
					}
					else
					{
						additionnal_right_margin_offset_ += 60;
						panel_bounds_.viewport.marginRight = data.marginRight + additionnal_right_margin_offset_;
					}
				}
				else if (data.name == "object_inspector_panel")
				{
					panel_bounds_.objectInspector.x = data.clientX;
					panel_bounds_.objectInspector.y = data.clientY;
					panel_bounds_.objectInspector.width = data.width;
					panel_bounds_.objectInspector.height = data.height;
				}
				else if (data.name == "history_panel")
				{
					panel_bounds_.history.x = data.clientX;
					panel_bounds_.history.y = data.clientY;
					panel_bounds_.history.width = data.width;
					panel_bounds_.history.height = data.height;
				}
				else if (data.name == "project_viewer_panel")
				{
					panel_bounds_.projectViewer.x = data.clientX;
					panel_bounds_.projectViewer.y = data.clientY;
					panel_bounds_.projectViewer.width = data.width;
					panel_bounds_.projectViewer.height = data.height;
				}
				else if (data.name == "panel_above_UI")
				{
					panel_bounds_.panelAboveUI.x = data.clientX;
					panel_bounds_.panelAboveUI.y = data.clientY;
					panel_bounds_.panelAboveUI.width = data.width;
					panel_bounds_.panelAboveUI.height = data.height;
				}
			}
		}

		std::string MouseController::detectMouseRegionFromClientCoordinates(int clientX, int clientY) const
		{
			PanelBounds panelBounds;
			{
				std::lock_guard<std::mutex> lock(g_panel_bounds_mutex);
				panelBounds = panel_bounds_;
			}

			if (!window_handle_)
			{
				return "Outside Window";
			}

			int windowWidth = 0;
			int windowHeight = 0;
			SDL_GetWindowSize(window_handle_, &windowWidth, &windowHeight);

			if (clientX < 0 || clientY < 0 || clientX >= windowWidth || clientY >= windowHeight)
			{
				return "Outside Window";
			}

			if (panelBounds.panelAboveUI.width > 0 && panelBounds.panelAboveUI.height > 0)
			{
				if (panelBounds.panelAboveUI.contains(clientX, clientY))
				{
					return "Panel Above UI";
				}
			}

			if (panelBounds.viewport.contains(clientX, clientY))
			{
				return "Viewport Panel";
			}

			if (panelBounds.hierarchy.contains(clientX, clientY))
			{
				return "Hierarchy Panel";
			}

			if (panelBounds.objectInspector.contains(clientX, clientY))
			{
				return "Object Inspector Panel";
			}

			if (panelBounds.history.contains(clientX, clientY))
			{
				return "History Panel";
			}

			if (panelBounds.projectViewer.contains(clientX, clientY))
			{
				return "Project Viewer Panel";
			}

			return "Splitter";
		}

		void MouseController::transitionMouseState(const std::string& regionName, int mouseX, int mouseY)
		{
			ViewportBounds viewportBounds;
			{
				std::lock_guard<std::mutex> lock(g_panel_bounds_mutex);
				viewportBounds = panel_bounds_.viewport;
			}

			last_detected_region_name_ = regionName;

 			Mouse_State* newState = current_mouse_state_;
			
			if (regionName == "Viewport Panel")
			{
				if (viewportBounds.contains(mouseX, mouseY))
				{
					newState = above_overlay_state_;
				}
				else
				{
					newState = outside_overlay_state_;
				}
			}
			else if (regionName == "Panel Above UI")
			{
				newState = above_ui_panel_state_;
			}
			else
			{
				newState = outside_overlay_state_;
			}

			if (newState != current_mouse_state_)
			{
				std::cout << "\n [UIHandler]---------------------------------------------- " << std::endl;
				std::cout << "Mouse transitionned at pos (" << mouseX << ", " << mouseY << ")" << std::endl;
					
				std::cout << "[Viewport Panel Bounds] Position: X=" << viewportBounds.x << " Y=" << viewportBounds.y
				<< " | Size: W=" << viewportBounds.width << " H=" << viewportBounds.height << std::endl;
					
				std::cout << "-------------------------------------------------------\n" << std::endl;


				if (current_mouse_state_)
				{
					current_mouse_state_->onExit();
				}

				current_mouse_state_ = newState;

				if (current_mouse_state_)
				{
					current_mouse_state_->onEnter();
				}
			}
		}

		bool MouseController::isShiftLeftClickActive()
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

		void MouseController::processMouseInput(UINT msg, WPARAM wParam)
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
		
		void MouseController::processMouseWheelInput(UINT msg, WPARAM wParam)
		{
			if (msg == WM_MOUSEWHEEL)
			{
				int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				last_wheel_input_time_ = GetTickCount();				
				
				if (delta > 0)
				{
					mouse_wheel_direction_ = 1;
				}
				else if (delta < 0)
				{
					mouse_wheel_direction_ = -1;
				}
				else
				{
					mouse_wheel_direction_ = 0;
				}
			}
		}
		
		void MouseController::updateWheelState()
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
		
		void MouseController::processMouseMove(int mouseX, int mouseY)
		{
			previous_mouse_x_ = current_mouse_x_;
			previous_mouse_y_ = current_mouse_y_;
			
			current_mouse_x_ = mouseX;
			current_mouse_y_ = mouseY;
			
			mouse_delta_x_ = current_mouse_x_ - previous_mouse_x_;
			mouse_delta_y_ = current_mouse_y_ - previous_mouse_y_;
			
			mouse_position_initialized_ = true;
		}
		
		void MouseController::updateMousePosition()
		{
			if (window_handle_)
			{
				float localMouseX, localMouseY;
				SDL_GetMouseState(&localMouseX, &localMouseY);
				
				int mouseX = static_cast<int>(std::lround(localMouseX));
				int mouseY = static_cast<int>(std::lround(localMouseY));
				
				processMouseMove(mouseX, mouseY);
				
				current_mouse_client_x_ = mouseX;
				current_mouse_client_y_ = mouseY;
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

			if (current_mouse_x_ != previous_mouse_x_ || current_mouse_y_ != previous_mouse_y_)
			{
				int viewportHeightThreshold = 0;
				int viewportTopOffset = 0;
				{
					std::lock_guard<std::mutex> lock(g_panel_bounds_mutex);
					viewportHeightThreshold = panel_bounds_.viewport.height;
					viewportTopOffset = panel_bounds_.viewport.y;
				}

				int displayMouseY = current_mouse_client_y_ - viewportTopOffset;

				if (displayMouseY < 0)
				{
					displayMouseY = 0;
				}

				if (viewportHeightThreshold > 0 && displayMouseY > viewportHeightThreshold)
				{
					displayMouseY = viewportHeightThreshold;
				}

				// std::cout << "Mouse pos is POS X :  " << current_mouse_x_  << " POS Y : " << displayMouseY << std::endl;
			}
 		}
		
		void MouseController::resetMousePosition()
		{
			mouse_position_initialized_ = false;
			mouse_delta_x_ = 0;
			mouse_delta_y_ = 0;
			current_mouse_client_x_ = 0;
			current_mouse_client_y_ = 0;
			smoothed_delta_x_ = 0.0f;
			smoothed_delta_y_ = 0.0f;
		}
		
		bool MouseController::hasClickEvent()
		{
			if (click_event_pending_)
			{
				click_event_pending_ = false;
				return true;
			}
			return false;
		}
		
		bool MouseController::isClickHeldForDuration(DWORD durationMs)
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
