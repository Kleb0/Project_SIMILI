#pragma once
#include <string>

class SDL_ApplicationWindow;

class SDL_State
{
public:
	explicit SDL_State(SDL_ApplicationWindow* owner) : owner_(owner) {}
	virtual ~SDL_State() = default;

	virtual void enter_state() { log_enter(); }
	virtual void leave_state() {}
	virtual SDL_State* return_state() { return nullptr; }

	virtual std::string name() const { return "Unknown"; }

protected:
	void log_enter();
	SDL_ApplicationWindow* owner_;
};
