#pragma once

#include "../FrameDatas/FrameDatas.hpp"
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
				const std::map<std::string, IFrameScreenData>& panelMap,
				int workSpaceBorderLeft, int workSpaceBorderTop,
				int workSpaceBorderWidth, int workSpaceBorderHeight,
				int splitterThickness = 0);

			void set(int x, int y, int width, int height);

			int getX() const;
			int getY() const;
			int getWidth() const;
			int getHeight() const;

			bool isValid() const;

		private:
			int x_;
			int y_;
			int width_;
			int height_;
		};

	}
}
