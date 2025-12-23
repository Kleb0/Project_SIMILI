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
			clientX = pt.x;
			clientY = pt.y;
			
			// Bounds are already adjusted with DPI and maximized offsets in updatePanelBoundsFromStocker()
			// So we don't apply offsets here anymore
		}

		void IFrameMouseDetector::getRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const
		{
			screenToClient(screenX, screenY, relativeX, relativeY);
			// Remove DPI scaling - not needed if JS sends correct client coordinates
		}

		void IFrameMouseDetector::getViewportRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const
		{
			int clientX, clientY;
			screenToClient(screenX, screenY, clientX, clientY);
			
			if (dpi_scale_ != 1.0f)
			{
				clientX = static_cast<int>(clientX / dpi_scale_);
				clientY = static_cast<int>(clientY / dpi_scale_);
			}
			
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
			
			// Work entirely in logical (CSS) pixels - no DPI scaling anywhere
			
			// DEBUG: Log mouse position and panel bounds
			static int debugCounter = 0;
			if (++debugCounter % 30 == 0)
			{
				std::cout << "[IFrameMouseDetector] Client position: (" << clientX << ", " << clientY << ")" << std::endl;
				std::cout << "  Viewport bounds: x=" << panel_bounds_.viewport.x 
						  << " y=" << panel_bounds_.viewport.y
						  << " w=" << panel_bounds_.viewport.width
						  << " h=" << panel_bounds_.viewport.height << std::endl;
				
				// Additional debug: show if mouse would be in viewport
				bool inViewport = panel_bounds_.viewport.contains(clientX, clientY);
				if (inViewport) {
					std::cout << "  -> INSIDE viewport" << std::endl;
				}
			}
			
			if (panel_bounds_.hierarchy.contains(clientX, clientY))
			{
				return MouseRegion::HierarchyPanel;
			}
			
			if (panel_bounds_.viewport.contains(clientX, clientY))
			{
				return MouseRegion::ViewportPanel;
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