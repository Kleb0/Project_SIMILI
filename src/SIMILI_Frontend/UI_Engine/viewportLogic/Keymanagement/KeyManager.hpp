#pragma once

#include <windows.h>
#include <functional>
#include <unordered_map>
#include <string>
#include <memory>
#include <imgui.h>

namespace SIMILI {
namespace Input {

// ============= ECS COMPONENTS =============

// Component: Key State
struct KeyStateComponent
{
	ImGuiKey imguiKey;           
	int windowsKey;             
	bool isPressed;
	bool wasPressed;
	bool isFirstPress;
	bool justReleased; 
	
	KeyStateComponent()
		: imguiKey(ImGuiKey_None)
		, windowsKey(0)
		, isPressed(false)
		, wasPressed(false)
		, isFirstPress(false)
		, justReleased(false)
	{}
};


struct KeyActionComponent
{
	std::string actionName;
	std::function<void()> callback; 
	bool triggerOnPress;
	
	KeyActionComponent()
		: actionName("")
		, callback(nullptr)
		, triggerOnPress(true)
	{}
};


class KeyInputSystem
{
public:
	KeyInputSystem();
	~KeyInputSystem();
	
	void processKeyDown(int windowsKey, LPARAM lParam);
	void processKeyUp(int windowsKey);
	
	void update();
	void pollKeyStates(); // Check real Windows key states immediately
	
	bool isKeyPressed(int windowsKey) const;
	bool wasKeyJustPressed(int windowsKey) const;
	bool wasKeyJustReleased(int windowsKey) const;
	
	void registerKey(int windowsKey, ImGuiKey imguiKey);
	
	const KeyStateComponent* getKeyState(int windowsKey) const;
	
private:
	std::unordered_map<int, KeyStateComponent> keyStates_;
};

class KeyActionSystem
{
public:
	KeyActionSystem(KeyInputSystem* inputSystem);
	~KeyActionSystem();
	
	void bindAction(int windowsKey, const std::string& actionName, std::function<void()> callback, bool triggerOnPress = true);
	
	void unbindAction(int windowsKey);
	
	void processActions();
	
private:
	KeyInputSystem* inputSystem_;
	std::unordered_map<int, KeyActionComponent> keyActions_;
};

// ============= MANAGER =============

// Manager: Orchestrates all key input systems
class KeyManager
{
public:
	static KeyManager& getInstance();
	
	void initialize();
	
	// Process Windows messages
	void handleKeyDown(int windowsKey, LPARAM lParam);
	void handleKeyUp(int windowsKey);
	
	void update();
	
	void registerStandardKeys();
	
	// Bind actions for 3D mode switching and Gizmo transforms
	void bindGizmoActions(std::function<void()> onTranslate, std::function<void()> onRotate, std::function<void()> onScale);
	void bindModeActions(std::function<void(int)> onModeSwitch);
	
	// Query system
	KeyInputSystem* getInputSystem() { return inputSystem_.get(); }
	KeyActionSystem* getActionSystem() { return actionSystem_.get(); }
	
	// Utility: Check if Shift is pressed (for multi-selection)
	bool isShiftPressed() const {
		if (!initialized_ || !inputSystem_) return false;
		return inputSystem_->isKeyPressed(VK_LSHIFT) || inputSystem_->isKeyPressed(VK_RSHIFT);
	}
	
	// Send ImGui key events (for integration)
	void sendToImGui(int windowsKey, bool isDown);
	
private:
	KeyManager();
	~KeyManager();
	KeyManager(const KeyManager&) = delete;
	KeyManager& operator=(const KeyManager&) = delete;
	
	std::unique_ptr<KeyInputSystem> inputSystem_;
	std::unique_ptr<KeyActionSystem> actionSystem_;
	bool initialized_;
};

} // namespace Input
} // namespace SIMILI
