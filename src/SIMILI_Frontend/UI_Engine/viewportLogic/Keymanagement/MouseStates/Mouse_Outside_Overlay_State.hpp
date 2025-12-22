#pragma once

#include "Mouse_State.hpp"

namespace SIMILI
{
	namespace Input
	{
		class Mouse_Outside_Overlay_State : public Mouse_State
		{
		public:
			Mouse_Outside_Overlay_State();
			virtual ~Mouse_Outside_Overlay_State() = default;

			// Override state behavior methods
			virtual void onEnter() override;
			virtual void onExit() override;
			virtual void update() override;

			// Specific getters
			bool isOutsideWindow() const;
			bool shouldBlockInput() const;

			// Specific setters
			void setOutsideWindow(bool outside);
			void setShouldBlockInput(bool block);

		private:
			bool is_outside_window_;
			bool should_block_input_;
		};
	}
}