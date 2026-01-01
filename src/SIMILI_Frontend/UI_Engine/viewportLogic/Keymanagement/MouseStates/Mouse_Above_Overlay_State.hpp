#pragma once

#include "Mouse_State.hpp"

namespace SIMILI
{
	namespace Input
	{
		class Mouse_Above_Overlay_State : public Mouse_State
		{
		public:
			Mouse_Above_Overlay_State();
			virtual ~Mouse_Above_Overlay_State() = default;

			// Override state behavior methods
			virtual void onEnter() override;
			virtual void onExit() override;
			virtual void update() override;		virtual const char* getStateName() const override;
			// Specific getters
			bool isOverViewport() const;
			bool canInteract() const;

			// Specific setters
			void setOverViewport(bool over);
			void setCanInteract(bool interact);

		private:
			bool is_over_viewport_;
			bool can_interact_;
		};
	}
}