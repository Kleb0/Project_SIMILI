#pragma once

#include "CEF_Drawer.hpp"

class CEF_Resizer
{
public:
	explicit CEF_Resizer(CEF_Drawer& owner);

	void resize(int width, int height);
	void ensureTextureStorage(int width, int height);
	bool rebuildUIPanelTextureLocked(const std::string& panelName, CEF_Drawer::UIPanelTextureData& textureData, CEF_Drawer::UIPanelFrameData& outFrame);
	bool translateMousePosition(float inputX, float inputY, int& outputX, int& outputY);
	void flushRuntimeLayoutSync();
	std::string buildRuntimeLayoutSyncScript(const std::map<std::string, CEF_Drawer::UIPanelFrameData>& panelFrames) const;
	void forceLayoutSync();

private:
	CEF_Drawer& owner_;
};
