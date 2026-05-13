#pragma once

class SDL_ApplicationWindow;

class SDL_State
{
public:
	explicit SDL_State(SDL_ApplicationWindow* owner) : owner_(owner) {}
	virtual ~SDL_State() = default;

	virtual void enter_state() {}
	virtual void leave_state() {}
	virtual SDL_State* return_state() { return nullptr; }

protected:
	SDL_ApplicationWindow* owner_;
};
