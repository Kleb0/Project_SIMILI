#pragma once

namespace SIMILI
{
	namespace Input
	{
		class Mouse_State
		{
		public:
			Mouse_State();
			virtual ~Mouse_State() = default;

			// Virtual methods for state behavior
			virtual void onEnter() = 0;
			virtual void onExit() = 0;
			virtual void update() = 0;
		virtual const char* getStateName() const = 0;

		// Getters
		virtual bool isActive() const;
		// Setters
		virtual void setActive(bool active);

	protected:
		bool is_active_;		};
	}
}