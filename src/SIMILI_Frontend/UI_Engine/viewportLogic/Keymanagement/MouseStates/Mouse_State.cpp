#include "Mouse_State.hpp"

namespace SIMILI
{
	namespace Input
	{
		Mouse_State::Mouse_State()
			: is_active_(false)
		{
		}

		bool Mouse_State::isActive() const
		{
			return is_active_;
		}

		void Mouse_State::setActive(bool active)
		{
			is_active_ = active;
		}
	}
}