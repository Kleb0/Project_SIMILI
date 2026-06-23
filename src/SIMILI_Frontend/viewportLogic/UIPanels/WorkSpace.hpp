#pragma once

#include "../FrameDatas/IFrameDatas.hpp"
#include <vulkan/vulkan.h>
#include <map>
#include <string>

namespace SIMILI {
	namespace Frontend {

		class WorkSpace
		{
		public:
			WorkSpace();
			~WorkSpace() = default;

			void computeFromPanels(
				int workSpaceBorderLeft, int workSpaceBorderTop,
				int workSpaceBorderWidth, int workSpaceBorderHeight,
				int splitterThickness = 0);

			void set(int x, int y, int width, int height);
			void setBounds(int left, int top, int right, int bottom);

			// Renders a solid 2D canvas in the workspace area (for Drawing workspace).
			void Render2DDrawingScreen(VkCommandBuffer commandBuffer, float dpiScale = 1.0f) const;

			int getX() const;      // == left
			int getY() const;      // == top
			int getWidth() const;  // == right - left
			int getHeight() const; // == bottom - top
			int getRight() const;
			int getBottom() const;

			bool isValid() const;

		private:
			int left_;
			int top_;
			int right_;
			int bottom_;
		};

	}
}