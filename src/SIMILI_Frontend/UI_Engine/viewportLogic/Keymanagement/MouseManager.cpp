#include "MouseManager.hpp"
#include <windowsx.h>
#include <iostream>

namespace SIMILI {
namespace Input {

// ============= MOUSE INPUT SYSTEM =============

void MouseInputSystem::processMouseMove(MouseStateComponent& state, int x, int y)
{
	state.x = x;
	state.y = y;
}

void MouseInputSystem::processMouseButton(MouseStateComponent& state, MouseButton button, bool pressed)
{
	switch (button)
	{
		case MouseButton::Left:
			state.leftPressed = pressed;
			break;
		case MouseButton::Right:
			state.rightPressed = pressed;
			break;
		case MouseButton::Middle:
			state.middlePressed = pressed;
			break;
	}
}

void MouseInputSystem::processMouseWheel(MouseStateComponent& state, int delta)
{
	state.wheelDelta = delta;
}

void MouseInputSystem::update(MouseStateComponent& state)
{
	// Update "was pressed" states for next frame
	state.leftWasPressed = state.leftPressed;
	state.rightWasPressed = state.rightPressed;
	state.middleWasPressed = state.middlePressed;
	
	// Reset wheel delta after each frame
	state.wheelDelta = 0;
}

// ============= MOUSE ACTION SYSTEM =============

void MouseActionSystem::processActions(const MouseStateComponent& state, 
                                        const std::unordered_map<std::string, MouseActionComponent>& actions)
{
	for (const auto& [name, action] : actions)
	{
		bool shouldTrigger = false;
		bool isPressed = false;
		bool wasPressed = false;
		
		switch (action.button)
		{
			case MouseButton::Left:
				isPressed = state.leftPressed;
				wasPressed = state.leftWasPressed;
				break;
			case MouseButton::Right:
				isPressed = state.rightPressed;
				wasPressed = state.rightWasPressed;
				break;
			case MouseButton::Middle:
				isPressed = state.middlePressed;
				wasPressed = state.middleWasPressed;
				break;
		}
		
		if (action.triggerOnPress)
		{
			// Trigger on first press
			shouldTrigger = isPressed && !wasPressed;
		}
		else
		{
			// Trigger on release
			shouldTrigger = !isPressed && wasPressed;
		}
		
		if (shouldTrigger && action.callback)
		{
			action.callback(state.x, state.y);
		}
	}
}

// ============= MOUSE MANAGER =============

MouseManager& MouseManager::getInstance()
{
	static MouseManager instance;
	return instance;
}

MouseManager::MouseManager()
	: mouse_state_()
	, input_system_()
	, action_system_()
	, actions_()
{
}

void MouseManager::handleMouseMove(int x, int y)
{
	input_system_.processMouseMove(mouse_state_, x, y);
}

void MouseManager::handleMouseButtonDown(MouseButton button, int x, int y)
{
	input_system_.processMouseMove(mouse_state_, x, y);
	input_system_.processMouseButton(mouse_state_, button, true);
}

void MouseManager::handleMouseButtonUp(MouseButton button)
{
	input_system_.processMouseButton(mouse_state_, button, false);
}

void MouseManager::handleMouseWheel(int delta)
{
	input_system_.processMouseWheel(mouse_state_, delta);
}

// Windows message handling helpers
void MouseManager::handleLButtonDown(WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	handleMouseButtonDown(MouseButton::Left, x, y);
}

void MouseManager::handleLButtonUp(WPARAM wParam, LPARAM lParam)
{
	handleMouseButtonUp(MouseButton::Left);
}

void MouseManager::handleRButtonDown(WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	handleMouseButtonDown(MouseButton::Right, x, y);
}

void MouseManager::handleRButtonUp(WPARAM wParam, LPARAM lParam)
{
	handleMouseButtonUp(MouseButton::Right);
}

void MouseManager::handleMButtonDown(WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	handleMouseButtonDown(MouseButton::Middle, x, y);
}

void MouseManager::handleMButtonUp(WPARAM wParam, LPARAM lParam)
{
	handleMouseButtonUp(MouseButton::Middle);
}

void MouseManager::handleWMMouseMove(WPARAM wParam, LPARAM lParam)
{
	int x = GET_X_LPARAM(lParam);
	int y = GET_Y_LPARAM(lParam);
	handleMouseMove(x, y);
}

void MouseManager::handleWMMouseWheel(WPARAM wParam, LPARAM lParam)
{
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);
	handleMouseWheel(delta);
}

void MouseManager::registerAction(const std::string& actionName, 
                                   MouseButton button,
                                   std::function<void(int, int)> callback,
                                   bool triggerOnPress)
{
	MouseActionComponent action;
	action.actionName = actionName;
	action.button = button;
	action.callback = callback;
	action.triggerOnPress = triggerOnPress;
	
	actions_[actionName] = action;
	
	std::cout << "[MouseManager] Registered action: " << actionName << std::endl;
}

void MouseManager::unregisterAction(const std::string& actionName)
{
	actions_.erase(actionName);
	std::cout << "[MouseManager] Unregistered action: " << actionName << std::endl;
}

bool MouseManager::isButtonPressed(MouseButton button) const
{
	switch (button)
	{
		case MouseButton::Left:
			return mouse_state_.leftPressed;
		case MouseButton::Right:
			return mouse_state_.rightPressed;
		case MouseButton::Middle:
			return mouse_state_.middlePressed;
	}
	return false;
}

bool MouseManager::wasButtonJustPressed(MouseButton button) const
{
	switch (button)
	{
		case MouseButton::Left:
			return mouse_state_.leftPressed && !mouse_state_.leftWasPressed;
		case MouseButton::Right:
			return mouse_state_.rightPressed && !mouse_state_.rightWasPressed;
		case MouseButton::Middle:
			return mouse_state_.middlePressed && !mouse_state_.middleWasPressed;
	}
	return false;
}

bool MouseManager::wasButtonJustReleased(MouseButton button) const
{
	switch (button)
	{
		case MouseButton::Left:
			return !mouse_state_.leftPressed && mouse_state_.leftWasPressed;
		case MouseButton::Right:
			return !mouse_state_.rightPressed && mouse_state_.rightWasPressed;
		case MouseButton::Middle:
			return !mouse_state_.middlePressed && mouse_state_.middleWasPressed;
	}
	return false;
}

void MouseManager::getMousePosition(int& x, int& y) const
{
	x = mouse_state_.x;
	y = mouse_state_.y;
}

int MouseManager::getWheelDelta() const
{
	return mouse_state_.wheelDelta;
}

void MouseManager::update()
{
	// Process all registered actions
	action_system_.processActions(mouse_state_, actions_);
	
	// Update state for next frame
	input_system_.update(mouse_state_);
}

} // namespace Input
} // namespace SIMILI
