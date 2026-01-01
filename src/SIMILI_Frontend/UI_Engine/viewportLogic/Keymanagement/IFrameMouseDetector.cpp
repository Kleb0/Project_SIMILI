#include "IFrameMouseDetector.hpp"
#include <iostream>

namespace SIMILI {
	namespace Input {

		IFrameMouseDetector::IFrameMouseDetector()
			: window_handle_(nullptr)
			, window_rect_{}
			, panel_bounds_{}
			, dpi_scale_(1.0f)
			, is_maximized_(false)
			, maximized_offset_x_(0)
			, maximized_offset_y_(0)
		{
		}

		void IFrameMouseDetector::setWindowHandle(HWND hwnd)
		{
			window_handle_ = hwnd;
			updateWindowRect();
			updateDpiScale();
		}

		void IFrameMouseDetector::updatePanelBounds(const PanelBounds& bounds)
		{
			panel_bounds_ = bounds;
		}

		void IFrameMouseDetector::updatePanelBoundsFromFrameData(const std::map<std::string, IFrameScreenDataSimple>& frameDataMap)
		{
			// Update panel bounds from frame data map
			for (const auto& pair : frameDataMap)
			{
				const IFrameScreenDataSimple& data = pair.second;
				
				if (data.name == "hierarchy_inspector")
				{
					panel_bounds_.hierarchy.x = data.clientX;
					panel_bounds_.hierarchy.y = data.clientY;
					panel_bounds_.hierarchy.width = data.width;
					panel_bounds_.hierarchy.height = data.height;
				}
				else if (data.name == "viewport_docking")
				{
					panel_bounds_.viewport.x = data.clientX;
					panel_bounds_.viewport.y = data.clientY;
					panel_bounds_.viewport.width = data.width;
					panel_bounds_.viewport.height = data.height;
				}
				else if (data.name == "object_inspector")
				{
					panel_bounds_.objectInspector.x = data.clientX;
					panel_bounds_.objectInspector.y = data.clientY;
					panel_bounds_.objectInspector.width = data.width;
					panel_bounds_.objectInspector.height = data.height;
				}
				else if (data.name == "history_logger")
				{
					panel_bounds_.history.x = data.clientX;
					panel_bounds_.history.y = data.clientY;
					panel_bounds_.history.width = data.width;
					panel_bounds_.history.height = data.height;
				}
				else if (data.name == "project_viewer")
				{
					panel_bounds_.projectViewer.x = data.clientX;
					panel_bounds_.projectViewer.y = data.clientY;
					panel_bounds_.projectViewer.width = data.width;
					panel_bounds_.projectViewer.height = data.height;
				}
			}			
		}

		void IFrameMouseDetector::updateWindowRect()
		{
			if (window_handle_)
			{
				GetWindowRect(window_handle_, &window_rect_);
			}
		}

		void IFrameMouseDetector::updateDpiScale()
		{
			if (!window_handle_)
			{
				dpi_scale_ = 1.0f;
				return;
			}

			HDC hdc = GetDC(window_handle_);
			if (hdc)
			{
				int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
				dpi_scale_ = dpiX / 96.0f;
				ReleaseDC(window_handle_, hdc);
			}
			else
			{
				dpi_scale_ = 1.0f;
			}
		}

		float IFrameMouseDetector::getDpiScale() const
		{
			return dpi_scale_;
		}

		bool IFrameMouseDetector::isMouseInsideWindow(int screenX, int screenY) const
		{
			if (!window_handle_)
			{
				return false;
			}
			
			RECT rect = window_rect_;
			if (window_handle_)
			{
				GetWindowRect(window_handle_, &rect);
			}
			
			return screenX >= rect.left && screenX < rect.right &&
				screenY >= rect.top && screenY < rect.bottom;
		}

		bool IFrameMouseDetector::isMouseOnViewport(int screenX, int screenY) const
		{
			int clientX, clientY;
			screenToClient(screenX, screenY, clientX, clientY);
			
			return panel_bounds_.viewport.contains(clientX, clientY);
		}

		void IFrameMouseDetector::setMaximizedState(bool isMaximized, int offsetX, int offsetY)
		{
			is_maximized_ = isMaximized;
			maximized_offset_x_ = offsetX;
			maximized_offset_y_ = offsetY;
			
			if (isMaximized) {
				std::cout << "[IFrameMouseDetector] Window maximized - applying offsets: X=" 
						  << offsetX << " Y=" << offsetY << std::endl;
			} else {
				std::cout << "[IFrameMouseDetector] Window not maximized - no offsets" << std::endl;
			}
		}

		void IFrameMouseDetector::screenToClient(int screenX, int screenY, int& clientX, int& clientY) const
		{
			if (!window_handle_)
			{
				clientX = screenX;
				clientY = screenY;
				return;
			}
			
			POINT pt = { screenX, screenY };
			ScreenToClient(window_handle_, &pt);
				
			clientX = static_cast<int>(pt.x / dpi_scale_);
			clientY = static_cast<int>(pt.y / dpi_scale_);
		}

		void IFrameMouseDetector::getRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const
		{
			screenToClient(screenX, screenY, relativeX, relativeY);
		}

		void IFrameMouseDetector::getViewportRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const
		{
			int clientX, clientY;
			screenToClient(screenX, screenY, clientX, clientY);
			
			relativeX = clientX - panel_bounds_.viewport.x;
			relativeY = clientY - panel_bounds_.viewport.y;
		}

		MouseRegion IFrameMouseDetector::detectMouseRegion(int screenX, int screenY)
		{
			updateWindowRect();
			
			if (!isMouseInsideWindow(screenX, screenY))
			{
				return MouseRegion::Outside;
			}
			
			int clientX, clientY;
			screenToClient(screenX, screenY, clientX, clientY);			

			if (panel_bounds_.viewport.contains(clientX, clientY))
			{
				return MouseRegion::ViewportPanel;
			}
			
			if (panel_bounds_.hierarchy.contains(clientX, clientY))
			{
				return MouseRegion::HierarchyPanel;
			}
			
			if (panel_bounds_.objectInspector.contains(clientX, clientY))
			{
				return MouseRegion::ObjectInspectorPanel;
			}
			
			if (panel_bounds_.history.contains(clientX, clientY))
			{
				return MouseRegion::HistoryPanel;
			}
			
			if (panel_bounds_.projectViewer.contains(clientX, clientY))
			{
				return MouseRegion::ProjectViewerPanel;
			}
			
			return MouseRegion::Splitter;
		}
	} 
}
