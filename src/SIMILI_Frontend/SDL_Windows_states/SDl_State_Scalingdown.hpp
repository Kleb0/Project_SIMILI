#pragma once
#include "SDL_State.hpp"

class SDL_State_ScalingDown : public SDL_State
{

public:
	explicit SDL_State_ScalingDown(SDL_ApplicationWindow* owner) : SDL_State(owner) {}
	~SDL_State_ScalingDown() override = default;

	void enter_state() override { SDL_State::enter_state(); }
	void leave_state() override {}
	SDL_State* return_state() override { return this; }
	std::string name() const override { return "ScalingDown"; }
};
