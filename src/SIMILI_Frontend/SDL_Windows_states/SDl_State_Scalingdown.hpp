#pragma once
#include "SDL_State.hpp"

class SDL_State_ScaleDown : public SDL_State
{
public:
	explicit SDL_State_ScaleDown(SDL_ApplicationWindow* owner) : SDL_State(owner) {}
	~SDL_State_ScaleDown() override = default;

	void enter_state() override {}
	void leave_state() override {}
	SDL_State* return_state() override { return this; }
};
