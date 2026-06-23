#include "WorkSpace.hpp"
#include <algorithm>
#include <limits>

namespace SIMILI {
	namespace Frontend {

		WorkSpace::WorkSpace()
			: left_(0)
			, top_(0)
			, right_(0)
			, bottom_(0)
		{
		}

		// void WorkSpace::computeFromPanels(
		// 	const std::map<std::string, IFrameScreenData>& panelMap,
		// 	int workSpaceBorderLeft, int workSpaceBorderTop,
		// 	int workSpaceBorderWidth, int workSpaceBorderHeight,
		// 	int splitterThickness)
		// {
		// 	if (panelMap.empty())
		// 	{
		// 		left_ = workSpaceBorderLeft;
		// 		top_  = workSpaceBorderTop;
		// 		right_ = workSpaceBorderLeft + workSpaceBorderWidth;
		// 		bottom_ = workSpaceBorderTop  + workSpaceBorderHeight;
		// 		return;
		// 	}

		// 	int workSpaceBorderRight = workSpaceBorderLeft + workSpaceBorderWidth;
		// 	int workSpaceBorderBottom = workSpaceBorderTop + workSpaceBorderHeight;

		// 	int leftBound = workSpaceBorderLeft;
		// 	int rightBound = workSpaceBorderRight;
		// 	int topBound = workSpaceBorderTop;
		// 	int bottomBound = workSpaceBorderBottom;

		// 	// Offset = splitter thickness minus the debug border thickness (3px),
		// 	// so the debug rectangle sits just inside the splitter without overlapping.
		// 	const int debugBorderThickness = 3;
		// 	const int offset = (splitterThickness > debugBorderThickness)
		// 		? (splitterThickness - debugBorderThickness)
		// 		: 0;

		// 	const float fullWidthThreshold = 0.85f;
		// 	const int borderRight  = workSpaceBorderLeft + workSpaceBorderWidth;
		// 	const int borderBottom = workSpaceBorderTop  + workSpaceBorderHeight;

		// 	int minColumnPanelY = std::numeric_limits<int>::max();

		// 	for (const auto& pair : panelMap)
		// 	{
		// 		const IFrameScreenData& panel = pair.second;
		// 		float widthRatio = (workSpaceBorderWidth > 0)
		// 			? (panel.width / (float)workSpaceBorderWidth)
		// 			: 0.0f;

		// 		if (widthRatio >= fullWidthThreshold)
		// 		{
		// 			// Row panel - anchor side = whichever boundary edge is closest
		// 			int topDist = panel.relativeY - workSpaceBorderTop;
		// 			int bottomDist = borderBottom - (panel.relativeY + panel.height);
		// 			if (topDist <= bottomDist)
		// 			{
		// 				int panelBottom = panel.relativeY + panel.height;
		// 				topBound = std::max(topBound, panelBottom + offset);
		// 			}
		// 			else
		// 			{
		// 				bottomBound = std::min(bottomBound, panel.relativeY - offset);
		// 			}
		// 		}
		// 		else
		// 		{
		// 			// Column panel - anchor side = whichever boundary edge is closest
		// 			int leftDist  = panel.relativeX - workSpaceBorderLeft;
		// 			int rightDist = borderRight - (panel.relativeX + panel.width);
		// 			if (leftDist <= rightDist)
		// 			{
		// 				int panelRight = panel.relativeX + panel.width;
		// 				leftBound = std::max(leftBound, panelRight + offset);
		// 			}
		// 			else
		// 			{
		// 				rightBound = std::min(rightBound, panel.relativeX - offset);
		// 			}
		// 			minColumnPanelY = std::min(minColumnPanelY, panel.relativeY);
		// 		}
		// 	}

		// 	if (minColumnPanelY != std::numeric_limits<int>::max())
		// 		topBound = std::max(topBound, minColumnPanelY);

		// 	left_   = leftBound;
		// 	top_    = topBound;
		// 	right_  = rightBound;
		// 	bottom_ = bottomBound;
		// }

		int WorkSpace::getX() const
		{
			return left_;
		}

		int WorkSpace::getY() const
		{
			return top_;
		}

		int WorkSpace::getWidth() const
		{
			return right_ - left_;
		}

		int WorkSpace::getHeight() const
		{
			return bottom_ - top_;
		}

		int WorkSpace::getRight() const
		{
			return right_;
		}

		int WorkSpace::getBottom() const
		{
			return bottom_;
		}

		bool WorkSpace::isValid() const
		{
			return right_ > left_ && bottom_ > top_;
		}

		void WorkSpace::set(int x, int y, int width, int height)
		{
			left_   = x;
			top_    = y;
			right_  = x + width;
			bottom_ = y + height;
		}

		void WorkSpace::setBounds(int left, int top, int right, int bottom)
		{
			left_   = left;
			top_    = top;
			right_  = right;
			bottom_ = bottom;
		}

		void WorkSpace::Render2DDrawingScreen(VkCommandBuffer commandBuffer, float dpiScale) const
		{
			if (!isValid() || commandBuffer == VK_NULL_HANDLE)
				return;

			const float scale = (dpiScale > 0.0f) ? dpiScale : 1.0f;

			// Clear the workspace rect to a neutral dark canvas colour
			VkClearAttachment clearAttachment{};
			clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			clearAttachment.colorAttachment = 0;
			clearAttachment.clearValue.color = {{ 1.0f, 1.0f, 1.0f, 1.0f }};

			VkClearRect clearRect{};
			clearRect.rect.offset.x = static_cast<int32_t>(left_ * scale);
			clearRect.rect.offset.y = static_cast<int32_t>(top_ * scale);
			clearRect.rect.extent.width  = static_cast<uint32_t>((right_ - left_) * scale);
			clearRect.rect.extent.height = static_cast<uint32_t>((bottom_ - top_) * scale);
			clearRect.baseArrayLayer = 0;
			clearRect.layerCount = 1;

			vkCmdClearAttachments(commandBuffer, 1, &clearAttachment, 1, &clearRect);
		}

	}
}
