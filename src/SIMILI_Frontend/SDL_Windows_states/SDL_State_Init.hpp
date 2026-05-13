#pragma once
#include "SDL_State.hpp"

class SDL_State_Init : public SDL_State
{
public:
	explicit SDL_State_Init(SDL_ApplicationWindow* owner) : SDL_State(owner) {}
	~SDL_State_Init() override = default;

	void enter_state() override {}
	void leave_state() override {}
	SDL_State* return_state() override { return this; }
};
