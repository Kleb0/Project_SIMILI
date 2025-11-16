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
	char windowsKey;             
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
	
	void processKeyDown(char windowsKey, LPARAM lParam);
	void processKeyUp(char windowsKey);
	
	void update();
	void pollKeyStates(); // Check real Windows key states immediately
	
	bool isKeyPressed(char windowsKey) const;
	bool wasKeyJustPressed(char windowsKey) const;
	bool wasKeyJustReleased(char windowsKey) const;
	
	void registerKey(char windowsKey, ImGuiKey imguiKey);
	
	const KeyStateComponent* getKeyState(char windowsKey) const;
	
private:
	std::unordered_map<char, KeyStateComponent> keyStates_;
};

class KeyActionSystem
{
public:
	KeyActionSystem(KeyInputSystem* inputSystem);
	~KeyActionSystem();
	
	void bindAction(char windowsKey, const std::string& actionName, std::function<void()> callback, bool triggerOnPress = true);
	
	// Unbind an action
	void unbindAction(char windowsKey);
	
	void processActions();
	
private:
	KeyInputSystem* inputSystem_;
	std::unordered_map<char, KeyActionComponent> keyActions_;
};

// ============= MANAGER =============

// Manager: Orchestrates all key input systems
class KeyManager
{
public:
	static KeyManager& getInstance();
	
	void initialize();
	
	// Process Windows messages
	void handleKeyDown(char windowsKey, LPARAM lParam);
	void handleKeyUp(char windowsKey);
	
	void update();
	
	void registerStandardKeys();
	
	// Bind actions for 3D mode switching and Gizmo transforms
	void bindGizmoActions(std::function<void()> onTranslate, std::function<void()> onRotate, std::function<void()> onScale);
	void bindModeActions(std::function<void(int)> onModeSwitch);
	
	// Query system
	KeyInputSystem* getInputSystem() { return inputSystem_.get(); }
	KeyActionSystem* getActionSystem() { return actionSystem_.get(); }
	
	// Send ImGui key events (for integration)
	void sendToImGui(char windowsKey, bool isDown);
	
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
