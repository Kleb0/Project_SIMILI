#include "KeyManager.hpp"
#include <iostream>

namespace SIMILI {
namespace Input {

	KeyInputSystem::KeyInputSystem()
	{
	}

	KeyInputSystem::~KeyInputSystem()
	{
	}

	void KeyInputSystem::registerKey(int windowsKey, ImGuiKey imguiKey)
	{
		KeyStateComponent state;
		state.windowsKey = windowsKey;
		state.imguiKey = imguiKey;
		keyStates_[windowsKey] = state;
	}

	void KeyInputSystem::processKeyDown(int windowsKey, LPARAM lParam)
	{
		auto it = keyStates_.find(windowsKey);
		if (it == keyStates_.end())
		{
			std::cout << "[KeyInputSystem] Key " << windowsKey << " (char: " << (char)windowsKey << ") not registered - ignoring" << std::endl;
			return;
		}
		

		bool isRepeat = (lParam & (1 << 30)) != 0;

		if (!isRepeat && !it->second.isPressed)
		{
			std::cout << "[KeyInputSystem] Key " << windowsKey << " (char: " << (char)windowsKey << ") FIRST PRESS" << std::endl;
			it->second.isPressed = true;
			it->second.isFirstPress = true;
		}
	}

	void KeyInputSystem::processKeyUp(int windowsKey)
	{
		auto it = keyStates_.find(windowsKey);
		if (it == keyStates_.end())
			return;
		
		if (it->second.isPressed)
		{
			it->second.isPressed = false;
			it->second.justReleased = true;
		}
	}

	void KeyInputSystem::update()
	{
		for (auto& pair : keyStates_)
		{
			KeyStateComponent& state = pair.second;
			
			state.isFirstPress = false;
					if (state.justReleased)
			{
				state.justReleased = false;
			}
			
			state.wasPressed = state.isPressed;
		}
	}

	void KeyInputSystem::pollKeyStates()
	{
		// Poll actual Windows key states to detect missed releases
		for (auto& pair : keyStates_)
		{
			KeyStateComponent& state = pair.second;
			
			bool isActuallyPressed = (GetAsyncKeyState(state.windowsKey) & 0x8000) != 0;
			
			if (state.isPressed && !isActuallyPressed)
			{
				state.isPressed = false;
				state.justReleased = true;
			}
		}
	}

	bool KeyInputSystem::isKeyPressed(int windowsKey) const
	{
		auto it = keyStates_.find(windowsKey);
		bool result = (it != keyStates_.end()) ? it->second.isPressed : false;
		
		if (windowsKey == VK_LSHIFT || windowsKey == VK_RSHIFT) {
			std::cout << "\n [KeyInputSystem::isKeyPressed] TEST TEST TEST Key=" << windowsKey 
			          << " Found=" << (it != keyStates_.end()) 
			          << " isPressed=" << result;
			if (it != keyStates_.end()) {
				std::cout << " wasPressed=" << it->second.wasPressed;
			}
			std::cout << std::endl;
		}
		
		return result;
	}

	bool KeyInputSystem::wasKeyJustPressed(int windowsKey) const
	{
		auto it = keyStates_.find(windowsKey);
		return (it != keyStates_.end()) ? it->second.isFirstPress : false;
	}

	bool KeyInputSystem::wasKeyJustReleased(int windowsKey) const
	{
		auto it = keyStates_.find(windowsKey);
		return (it != keyStates_.end()) ? it->second.justReleased : false;
	}

	const KeyStateComponent* KeyInputSystem::getKeyState(int windowsKey) const
	{
		auto it = keyStates_.find(windowsKey);
		return (it != keyStates_.end()) ? &it->second : nullptr;
	}


	KeyActionSystem::KeyActionSystem(KeyInputSystem* inputSystem)
		: inputSystem_(inputSystem)
	{
	}

	KeyActionSystem::~KeyActionSystem()
	{
	}

	void KeyActionSystem::bindAction(int windowsKey, const std::string& actionName, std::function<void()> callback, bool triggerOnPress)
	{
		KeyActionComponent action;
		action.actionName = actionName;
		action.callback = callback;
		action.triggerOnPress = triggerOnPress;
		keyActions_[windowsKey] = action;
		
	}

	void KeyActionSystem::unbindAction(int windowsKey)
	{
		auto it = keyActions_.find(windowsKey);
		if (it != keyActions_.end())
		{
			keyActions_.erase(it);
		}
	}

	void KeyActionSystem::processActions()
	{
		for (auto& pair : keyActions_)
		{
			int key = pair.first;
			KeyActionComponent& action = pair.second;
			
			if (!action.callback)
				continue;
			
			if (action.triggerOnPress && inputSystem_->wasKeyJustPressed(key))
			{
				std::cout << "[KeyActionSystem] Triggering action: " << action.actionName << " (key: " << (char)key << ")" << std::endl;
				action.callback();
			}
			else if (!action.triggerOnPress && inputSystem_->wasKeyJustReleased(key))
			{
				std::cout << "[KeyActionSystem] Triggering action: " << action.actionName << " (key: " << (char)key << ") on release" << std::endl;
				action.callback();
			}
		}
	}


	KeyManager& KeyManager::getInstance()
	{
		static KeyManager instance;
		return instance;
	}

	KeyManager::KeyManager()
		: initialized_(false)
	{
	}

	KeyManager::~KeyManager()
	{
	}

	void KeyManager::initialize()
	{
		if (initialized_)
			return;
		
		inputSystem_ = std::make_unique<KeyInputSystem>();
		actionSystem_ = std::make_unique<KeyActionSystem>(inputSystem_.get());
		
		registerStandardKeys();
		
		initialized_ = true;
		std::cout << "[KeyManager] Initialized with ECS architecture" << std::endl;
	}

	void KeyManager::registerStandardKeys()
	{
		inputSystem_->registerKey('W', ImGuiKey_W);
		inputSystem_->registerKey('R', ImGuiKey_R);
		inputSystem_->registerKey('S', ImGuiKey_S);
		
		inputSystem_->registerKey('1', ImGuiKey_1);
		inputSystem_->registerKey('2', ImGuiKey_2);
		inputSystem_->registerKey('3', ImGuiKey_3);
		inputSystem_->registerKey('4', ImGuiKey_4);
		
		inputSystem_->registerKey(VK_LSHIFT, ImGuiKey_LeftShift);
		inputSystem_->registerKey(VK_RSHIFT, ImGuiKey_RightShift);
		
	}

	void KeyManager::handleKeyDown(int windowsKey, LPARAM lParam)
	{
		if (!initialized_)
			return;
		
		std::cout << "[KeyManager] handleKeyDown - key: " << windowsKey << " (char: " << (char)windowsKey << ")" << std::endl;
		
		inputSystem_->processKeyDown(windowsKey, lParam);
		sendToImGui(windowsKey, true);
	}

	void KeyManager::handleKeyUp(int windowsKey)
	{
		if (!initialized_)
			return;
		
		inputSystem_->processKeyUp(windowsKey);
		sendToImGui(windowsKey, false);
	}

	void KeyManager::update()
	{
		if (!initialized_)
			return;
		
		// IMPORTANT: Process actions BEFORE clearing the "just pressed" flags
		actionSystem_->processActions();
		
		inputSystem_->update();
	}

	void KeyManager::bindGizmoActions(std::function<void()> onTranslate, std::function<void()> onRotate, std::function<void()> onScale)
	{
		if (!initialized_)
			return;
		
		actionSystem_->bindAction('W', "GizmoTranslate", onTranslate, true);
		actionSystem_->bindAction('R', "GizmoRotate", onRotate, true);
		actionSystem_->bindAction('S', "GizmoScale", onScale, true);
		
		std::cout << "[KeyManager] Gizmo actions bound (W=Translate, R=Rotate, S=Scale)" << std::endl;
	}

	void KeyManager::bindModeActions(std::function<void(int)> onModeSwitch)
	{
		if (!initialized_)
			return;
		
		actionSystem_->bindAction('1', "SwitchMode1", [onModeSwitch]() { onModeSwitch(1); }, true);
		actionSystem_->bindAction('2', "SwitchMode2", [onModeSwitch]() { onModeSwitch(2); }, true);
		actionSystem_->bindAction('3', "SwitchMode3", [onModeSwitch]() { onModeSwitch(3); }, true);
		actionSystem_->bindAction('4', "SwitchMode4", [onModeSwitch]() { onModeSwitch(4); }, true);
		
		std::cout << "[KeyManager] Mode switch actions bound (1-4)" << std::endl;
	}

	void KeyManager::sendToImGui(int windowsKey, bool isDown)
	{
		const KeyStateComponent* state = inputSystem_->getKeyState(windowsKey);
		if (!state || state->imguiKey == ImGuiKey_None)
			return;
		
		ImGuiIO& io = ImGui::GetIO();
		io.AddKeyEvent(state->imguiKey, isDown);
	}

	} // namespace Input
} // namespace SIMILI
