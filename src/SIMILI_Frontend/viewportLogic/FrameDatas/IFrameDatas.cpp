#include "IFrameDatas.hpp"
#include <iostream>
#include <iomanip>

namespace SIMILI {
	namespace Frontend {

		IFrameDatas::IFrameDatas(FrameDataCatcher* catcher)
			: frame_data_catcher_(catcher)
		{
		}

		void IFrameDatas::setDatas(const std::string& name, int width, int height, int screenX, int screenY, int windowWidth, int windowHeight)
		{

			name_= name;
			width_ = width;
			height_ = height;
			screenX_ = screenX;
			screenY_ = screenY;
			windowWidth_ = windowWidth;
			windowHeight_ = windowHeight;
		}

		void IFrameDatas::getDatas(const std::string& name, int& width, int& height, int& screenX, int& screenY, int& windowWidth, int& windowHeight) 
		{
			if (name != name_)
			{
				std::cout << "[IFrameDatas] getDatas: No data found for name '" << name << "'" << std::endl;
				width = height = screenX = screenY = windowWidth = windowHeight = 0;
				return;
			}

			width = width_;
			height = height_;
			screenX = screenX_;
			screenY = screenY_;
			windowWidth = windowWidth_;
			windowHeight = windowHeight_;
		}


		void IFrameDatas::printAllFrameData() const
		{
			std::cout << "\n========== FrameDatas - Captured Frame Data ==========" << std::endl;




			std::cout << "\n========================================================\n" << std::endl;
		}

	}
}