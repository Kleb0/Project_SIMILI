#pragma once

#include <windows.h>
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>

namespace SIMILI {
namespace Input {

// ============= MOUSE BUTTON STATES =============

enum class MouseButton
{
	Left,
	Right,
	Middle
};

// ============= ECS COMPONENTS =============

// Component: Mouse State
struct MouseStateComponent
{
	int x;
	int y;
	int wheelDelta;
	bool leftPressed;
	bool rightPressed;
	bool middlePressed;
	bool leftWasPressed;
	bool rightWasPressed;
	bool middleWasPressed;
	
	MouseStateComponent()
		: x(0)
		, y(0)
		, wheelDelta(0)
		, leftPressed(false)
		, rightPressed(false)
		, middlePressed(false)
		, leftWasPressed(false)
		, rightWasPressed(false)
		, middleWasPressed(false)
	{}
};

// Component: Mouse Action
struct MouseActionComponent
{
	std::string actionName;
	std::function<void(int, int)> callback;  // Callback with mouse coordinates
	MouseButton button;
	bool triggerOnPress;  // true = on press, false = on release
	
	MouseActionComponent()
		: actionName("")
		, callback(nullptr)
		, button(MouseButton::Left)
		, triggerOnPress(true)
	{}
};

// ============= SYSTEMS =============

// System: Mouse Input Processing
class MouseInputSystem
{
public:
	void processMouseMove(MouseStateComponent& state, int x, int y);
	void processMouseButton(MouseStateComponent& state, MouseButton button, bool pressed);
	void processMouseWheel(MouseStateComponent& state, int delta);
	void update(MouseStateComponent& state);
};

// System: Mouse Action Processing
class MouseActionSystem
{
public:
	void processActions(const MouseStateComponent& state, 
	                    const std::unordered_map<std::string, MouseActionComponent>& actions);
};

// ============= MANAGER =============

class MouseManager
{
public:
	static MouseManager& getInstance();
	
	// Input handling
	void handleMouseMove(int x, int y);
	void handleMouseButtonDown(MouseButton button, int x, int y);
	void handleMouseButtonUp(MouseButton button);
	void handleMouseWheel(int delta);
	
	// Windows message handling helpers
	void handleLButtonDown(WPARAM wParam, LPARAM lParam);
	void handleLButtonUp(WPARAM wParam, LPARAM lParam);
	void handleRButtonDown(WPARAM wParam, LPARAM lParam);
	void handleRButtonUp(WPARAM wParam, LPARAM lParam);
	void handleMButtonDown(WPARAM wParam, LPARAM lParam);
	void handleMButtonUp(WPARAM wParam, LPARAM lParam);
	void handleWMMouseMove(WPARAM wParam, LPARAM lParam);
	void handleWMMouseWheel(WPARAM wParam, LPARAM lParam);
	
	// Action registration
	void registerAction(const std::string& actionName, 
	                    MouseButton button,
	                    std::function<void(int, int)> callback,
	                    bool triggerOnPress = true);
	void unregisterAction(const std::string& actionName);
	
	// State queries
	bool isButtonPressed(MouseButton button) const;
	bool wasButtonJustPressed(MouseButton button) const;
	bool wasButtonJustReleased(MouseButton button) const;
	void getMousePosition(int& x, int& y) const;
	int getWheelDelta() const;
	
	// Update (call once per frame)
	void update();

private:
	MouseManager();
	~MouseManager() = default;
	MouseManager(const MouseManager&) = delete;
	MouseManager& operator=(const MouseManager&) = delete;
	
	MouseStateComponent mouse_state_;
	MouseInputSystem input_system_;
	MouseActionSystem action_system_;
	std::unordered_map<std::string, MouseActionComponent> actions_;
};

} // namespace Input
} // namespace SIMILI
