#include "IFrameMouseDetector.hpp"
#include <iostream>

namespace SIMILI {
	namespace Input {

		IFrameMouseDetector::IFrameMouseDetector()
			: window_handle_(nullptr)
			, window_rect_{}
			, panel_bounds_{}
			, dpi_scale_(1.0f)
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
		}

		void IFrameMouseDetector::getRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const
		{
			screenToClient(screenX, screenY, relativeX, relativeY);
			
			if (dpi_scale_ != 1.0f)
			{
				relativeX = static_cast<int>(relativeX / dpi_scale_);
				relativeY = static_cast<int>(relativeY / dpi_scale_);
			}
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