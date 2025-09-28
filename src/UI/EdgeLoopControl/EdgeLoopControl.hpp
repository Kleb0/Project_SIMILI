
#pragma once
#include <imgui.h>

class EdgeLoopControl 
{
public:
	void show();
    void hide();
	void render();

private:
	bool isOpen = false;
};
