#include "WorkSpace.hpp"
#include <algorithm>
#include <limits>

namespace SIMILI {
	namespace Frontend {

		WorkSpace::WorkSpace()
			: x_(0)
			, y_(0)
			, width_(0)
			, height_(0)
		{
		}

		void WorkSpace::computeFromPanels(
			const std::map<std::string, IFrameScreenData>& panelMap,
			int workSpaceBorderLeft, int workSpaceBorderTop,
			int workSpaceBorderWidth, int workSpaceBorderHeight,
			int splitterThickness)
		{
			if (panelMap.empty())
			{
				x_ = workSpaceBorderLeft;
				y_ = workSpaceBorderTop;
				width_ = workSpaceBorderWidth;
				height_ = workSpaceBorderHeight;
				return;
			}

			int workSpaceBorderRight = workSpaceBorderLeft + workSpaceBorderWidth;
			int workSpaceBorderBottom = workSpaceBorderTop + workSpaceBorderHeight;

			int leftBound = workSpaceBorderLeft;
			int rightBound = workSpaceBorderRight;
			int topBound = workSpaceBorderTop;
			int bottomBound = workSpaceBorderBottom;

			// Offset = splitter thickness minus the debug border thickness (3px),
			// so the debug rectangle sits just inside the splitter without overlapping.
			const int debugBorderThickness = 3;
			const int offset = (splitterThickness > debugBorderThickness)
				? (splitterThickness - debugBorderThickness)
				: 0;

			const float fullWidthThreshold = 0.85f;
			const int borderRight  = workSpaceBorderLeft + workSpaceBorderWidth;
			const int borderBottom = workSpaceBorderTop  + workSpaceBorderHeight;

			for (const auto& pair : panelMap)
			{
				const IFrameScreenData& panel = pair.second;
				float widthRatio = (workSpaceBorderWidth > 0)
					? (panel.width / (float)workSpaceBorderWidth)
					: 0.0f;

				if (widthRatio >= fullWidthThreshold)
				{
					// Row panel - anchor side = whichever boundary edge is closest
					int topDist = panel.relativeY - workSpaceBorderTop;
					int bottomDist = borderBottom - (panel.relativeY + panel.height);
					if (topDist <= bottomDist)
					{
						int panelBottom = panel.relativeY + panel.height;
						topBound = std::max(topBound, panelBottom + offset);
					}
					else
					{
						bottomBound = std::min(bottomBound, panel.relativeY - offset);
					}
				}
				else
				{
					// Column panel - anchor side = whichever boundary edge is closest
					int leftDist  = panel.relativeX - workSpaceBorderLeft;
					int rightDist = borderRight - (panel.relativeX + panel.width);
					if (leftDist <= rightDist)
					{
						int panelRight = panel.relativeX + panel.width;
						leftBound = std::max(leftBound, panelRight + offset);
					}
					else
					{
						rightBound = std::min(rightBound, panel.relativeX - offset);
					}
				}
			}

			x_ = leftBound;
			y_ = topBound;
			width_  = rightBound - leftBound;
			height_ = bottomBound - topBound;
		}

		int WorkSpace::getX() const
		{
			return x_;
		}

		int WorkSpace::getY() const
		{
			return y_;
		}

		int WorkSpace::getWidth() const
		{
			return width_;
		}

		int WorkSpace::getHeight() const
		{
			return height_;
		}

		bool WorkSpace::isValid() const
		{
			return width_ > 0 && height_ > 0;
		}

		void WorkSpace::set(int x, int y, int width, int height)
		{
			x_ = x;
			y_ = y;
			width_ = width;
			height_ = height;
		}

	}
}
