#pragma once

#include <map>
#include <string>

class CEF_Drawer;

class CEF_Resizer
{
public:
	explicit CEF_Resizer(CEF_Drawer& owner);

	void resize(int width, int height);
	void ensureTextureStorage(int width, int height);
	
	struct UIPanelTextureData;
	struct UIPanelFrameData;
	
	bool rebuildUIPanelTextureLocked(const std::string& panelName, struct CEF_Drawer::UIPanelTextureData& textureData, struct CEF_Drawer::UIPanelFrameData& outFrame);
	bool translateMousePosition(float inputX, float inputY, int& outputX, int& outputY);
	void flushRuntimeLayoutSync();
	std::string buildRuntimeLayoutSyncScript(const std::map<std::string, struct CEF_Drawer::UIPanelFrameData>& panelFrames) const;
	void forceLayoutSync();

private:
	CEF_Drawer& owner_;
};
