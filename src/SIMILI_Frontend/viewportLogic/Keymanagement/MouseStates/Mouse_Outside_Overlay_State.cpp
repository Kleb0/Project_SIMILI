#include "Mouse_Outside_Overlay_State.hpp"
#include <iostream>

namespace SIMILI
{
	namespace Input
	{
		Mouse_Outside_Overlay_State::Mouse_Outside_Overlay_State()
			: Mouse_State()
			, is_outside_window_(true)
			, should_block_input_(true)
		{
		}

		void Mouse_Outside_Overlay_State::onEnter()
		{
			setActive(true);
			is_outside_window_ = true;
			should_block_input_ = true;
			std::cout << "[Mouse_Outside_Overlay_State] Entered - Mouse is outside overlay" << std::endl;
		}

		void Mouse_Outside_Overlay_State::onExit()
		{
			setActive(false);
			is_outside_window_ = false;
			should_block_input_ = false;
			std::cout << "[Mouse_Outside_Overlay_State] Exited" << std::endl;
		}

		void Mouse_Outside_Overlay_State::update()
		{
			// State update logic if needed
		}

		const char* Mouse_Outside_Overlay_State::getStateName() const
		{
			return "Mouse_Outside_Overlay_State";
		}

		bool Mouse_Outside_Overlay_State::isOutsideWindow() const
		{
			return is_outside_window_;
		}

		bool Mouse_Outside_Overlay_State::shouldBlockInput() const
		{
			return should_block_input_;
		}

		void Mouse_Outside_Overlay_State::setOutsideWindow(bool outside)
		{
			is_outside_window_ = outside;
		}

		void Mouse_Outside_Overlay_State::setShouldBlockInput(bool block)
		{
			should_block_input_ = block;
		}
	}
}