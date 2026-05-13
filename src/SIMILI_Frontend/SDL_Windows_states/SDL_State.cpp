#include "SDL_State.hpp"
#include <iostream>

void SDL_State::log_enter()
{
	std::cout << "Entering New SDL State ! Current SDL_State is : " << name() << std::endl;
}
