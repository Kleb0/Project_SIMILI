#include "Mouse_Above_Overlay_State.hpp"
#include <iostream>

namespace SIMILI
{
	namespace Input
	{
		Mouse_Above_Overlay_State::Mouse_Above_Overlay_State()
			: Mouse_State()
			, is_over_viewport_(false)
			, can_interact_(true)
		{
		}

		void Mouse_Above_Overlay_State::onEnter()
		{
			setActive(true);
			can_interact_ = true;
			std::cout << "[Mouse_Above_Overlay_State] Entered - Mouse is above overlay" << std::endl;
		}

		void Mouse_Above_Overlay_State::onExit()
		{
			setActive(false);
			can_interact_ = false;
			is_over_viewport_ = false;
			std::cout << "[Mouse_Above_Overlay_State] Exited" << std::endl;
		}

		void Mouse_Above_Overlay_State::update()
		{
			// State update logic if needed
		}

		bool Mouse_Above_Overlay_State::isOverViewport() const
		{
			return is_over_viewport_;
		}

		bool Mouse_Above_Overlay_State::canInteract() const
		{
			return can_interact_;
		}

		void Mouse_Above_Overlay_State::setOverViewport(bool over)
		{
			is_over_viewport_ = over;
		}

		void Mouse_Above_Overlay_State::setCanInteract(bool interact)
		{
			can_interact_ = interact;
		}
	}
}