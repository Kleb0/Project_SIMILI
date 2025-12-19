#pragma once

#include <windows.h>
#include <string>

namespace SIMILI 
{
	namespace Input 
	{

		enum class MouseRegion
		{
			Outside,
			HierarchyPanel,
			ViewportPanel,
			ObjectInspectorPanel,
			HistoryPanel,
			ProjectViewerPanel,
			Splitter,
			Unknown
		};

		struct ViewportBounds
		{
			int x;
			int y;
			int width;
			int height;
			
			ViewportBounds() : x(0), y(0), width(0), height(0) {}
			
			bool contains(int px, int py) const
			{
				return px >= x && px < (x + width) && py >= y && py < (y + height);
			}
		};

		struct PanelBounds
		{
			ViewportBounds hierarchy;
			ViewportBounds viewport;
			ViewportBounds objectInspector;
			ViewportBounds history;
			ViewportBounds projectViewer;
			
			PanelBounds() = default;
		};

		class IFrameMouseDetector
		{
			public:
				IFrameMouseDetector();
				~IFrameMouseDetector() = default;
				
				void setWindowHandle(HWND hwnd);
				void updatePanelBounds(const PanelBounds& bounds);
				
				MouseRegion detectMouseRegion(int screenX, int screenY);
				bool isMouseInsideWindow(int screenX, int screenY) const;
				bool isMouseOnViewport(int screenX, int screenY) const;
				
				void getRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const;
				void getViewportRelativePosition(int screenX, int screenY, int& relativeX, int& relativeY) const;
				
				const ViewportBounds& getViewportBounds() const { return panel_bounds_.viewport; }
				const PanelBounds& getPanelBounds() const { return panel_bounds_; }

			private:
				HWND window_handle_;
				RECT window_rect_;
				PanelBounds panel_bounds_;
				float dpi_scale_;
				
				void updateWindowRect();
				void updateDpiScale();
				float getDpiScale() const;
				void screenToClient(int screenX, int screenY, int& clientX, int& clientY) const;
		};
	} 
} 
