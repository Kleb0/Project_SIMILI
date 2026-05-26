#include "Mouse_Above_UI_Panel_State.hpp"
#include <iostream>

namespace SIMILI
{
	namespace Input
	{
		Mouse_Above_UI_Panel_State::Mouse_Above_UI_Panel_State()
			: Mouse_State()
			, is_over_ui_panel_(false)
			, should_block_viewport_interaction_(true)
		{
		}

		void Mouse_Above_UI_Panel_State::onEnter()
		{
			setActive(true);
			is_over_ui_panel_ = true;
			should_block_viewport_interaction_ = true;
			std::cout << "[Mouse_Above_UI_Panel_State] Entered - Mouse is above UI panel" << std::endl;
		}

		void Mouse_Above_UI_Panel_State::onExit()
		{
			setActive(false);
			is_over_ui_panel_ = false;
			should_block_viewport_interaction_ = false;
			std::cout << "[Mouse_Above_UI_Panel_State] Exited" << std::endl;
		}

		void Mouse_Above_UI_Panel_State::update()
		{
		}

		const char* Mouse_Above_UI_Panel_State::getStateName() const
		{
			return "Mouse_Above_UI_Panel_State";
		}

		bool Mouse_Above_UI_Panel_State::isOverUIPanel() const
		{
			return is_over_ui_panel_;
		}

		bool Mouse_Above_UI_Panel_State::shouldBlockViewportInteraction() const
		{
			return should_block_viewport_interaction_;
		}

		void Mouse_Above_UI_Panel_State::setOverUIPanel(bool over)
		{
			is_over_ui_panel_ = over;
		}

		void Mouse_Above_UI_Panel_State::setShouldBlockViewportInteraction(bool block)
		{
			should_block_viewport_interaction_ = block;
		}
	}
}
