#pragma once

#include "Mouse_State.hpp"

namespace SIMILI
{
	namespace Input
	{
		class Mouse_Above_UI_Panel_State : public Mouse_State
		{
		public:
			Mouse_Above_UI_Panel_State();
			virtual ~Mouse_Above_UI_Panel_State() = default;

			virtual void onEnter() override;
			virtual void onExit() override;
			virtual void update() override;
			virtual const char* getStateName() const override;

			bool isOverUIPanel() const;
			bool shouldBlockViewportInteraction() const;

			void setOverUIPanel(bool over);
			void setShouldBlockViewportInteraction(bool block);

		private:
			bool is_over_ui_panel_;
			bool should_block_viewport_interaction_;
		};
	}
}
