
#include "UI\EdgeLoopControl\EdgeLoopControl.hpp"

void EdgeLoopControl::show() 
{
	isOpen = true;
}

void EdgeLoopControl::hide() 
{
    isOpen = false;
}

void EdgeLoopControl::render() 
{
	if (isOpen) {
		ImGui::Begin("EdgeLoopControl", &isOpen, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("EdgeLoopControl");
		ImGui::End();
	}
}
