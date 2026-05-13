#pragma once
#include "SDL_State.hpp"

class SDL_State_ScaleUp : public SDL_State
{
public:
	explicit SDL_State_ScaleUp(SDL_ApplicationWindow* owner) : SDL_State(owner) {}
	~SDL_State_ScaleUp() override = default;

	void enter_state() override {}
	void leave_state() override {}
	SDL_State* return_state() override { return this; }
};
