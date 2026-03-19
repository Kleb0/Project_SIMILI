#include "CEF_Resizer.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "include/cef_browser.h"

CEF_Resizer::CEF_Resizer(CEF_Drawer& owner)
	: owner_(owner)
{
}

void CEF_Resizer::resize(int width, int height)
{
	{
		std::lock_guard<std::mutex> lock(owner_.render_mutex_);
		owner_.width_ = width;
		owner_.height_ = height;
		owner_.updateWindowProperties();
		ensureTextureStorage(owner_.width_, owner_.height_);
	}
	
	if (owner_.browser_)
	{
		CefRefPtr<CefBrowserHost> host = owner_.browser_->GetHost();
		if (host)
		{
			host->WasResized();
			host->Invalidate(PET_VIEW);
		}
	}
	
	std::cout << "[CEF_Drawer] Resized (logical pixels): " << width << "x" << height << std::endl;
}

void CEF_Resizer::ensureTextureStorage(int width, int height)
{
	if (!owner_.texture_id_ || width <= 0 || height <= 0)
	{
		return;
	}

	glBindTexture(GL_TEXTURE_2D, owner_.texture_id_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
}

bool CEF_Resizer::rebuildUIPanelTextureLocked(const std::string& panelName, CEF_Drawer::UIPanelTextureData& textureData, CEF_Drawer::UIPanelFrameData& outFrame)
{
	auto sourceIt = owner_.ui_panel_frames_.find(panelName);
	if (sourceIt == owner_.ui_panel_frames_.end())
	{
		return false;
	}

	CEF_Drawer::UIPanelFrameData sourceFrame = sourceIt->second;
	CEF_Drawer::UIPanelFrameData displayFrame = sourceFrame;

	auto displayIt = owner_.ui_panel_display_frames_.find(panelName);
	if (displayIt != owner_.ui_panel_display_frames_.end())
	{
		displayFrame = displayIt->second;
	}

	if (sourceFrame.width <= 0 || sourceFrame.height <= 0 || displayFrame.width <= 0 || displayFrame.height <= 0)
	{
		return false;
	}

	if (owner_.paint_buffer_.empty() || owner_.paint_buffer_width_ <= 0 || owner_.paint_buffer_height_ <= 0)
	{
		return false;
	}

	int sourceMinX = std::clamp(sourceFrame.x, 0, owner_.paint_buffer_width_ - 1);
	int sourceMinY = std::clamp(sourceFrame.y, 0, owner_.paint_buffer_height_ - 1);
	int sourceMaxX = std::clamp(sourceFrame.x + sourceFrame.width, sourceMinX + 1, owner_.paint_buffer_width_);
	int sourceMaxY = std::clamp(sourceFrame.y + sourceFrame.height, sourceMinY + 1, owner_.paint_buffer_height_);
	int sourceWidth = sourceMaxX - sourceMinX;
	int sourceHeight = sourceMaxY - sourceMinY;

	if (sourceWidth <= 0 || sourceHeight <= 0)
	{
		return false;
	}

	int targetWidth = sourceWidth > 0 ? sourceWidth : 1;
	int targetHeight = sourceHeight > 0 ? sourceHeight : 1;

	bool needsRebuild = textureData.dirty || textureData.texture_id == 0 || textureData.width != targetWidth || textureData.height != targetHeight;
	if (!needsRebuild)
	{
		outFrame.x = 0;
		outFrame.y = 0;
		outFrame.width = textureData.width;
		outFrame.height = textureData.height;
		return true;
	}

	std::vector<unsigned char> panelPixels(static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight) * 4u);

	for (int targetY = 0; targetY < targetHeight; ++targetY)
	{
		float normalizedY = (static_cast<float>(targetY) + 0.5f) / static_cast<float>(targetHeight);
		int sampleY = sourceMinY + static_cast<int>(std::floor(normalizedY * static_cast<float>(sourceHeight)));
		sampleY = std::clamp(sampleY, sourceMinY, sourceMaxY - 1);

		for (int targetX = 0; targetX < targetWidth; ++targetX)
		{
			float normalizedX = (static_cast<float>(targetX) + 0.5f) / static_cast<float>(targetWidth);
			int sampleX = sourceMinX + static_cast<int>(std::floor(normalizedX * static_cast<float>(sourceWidth)));
			sampleX = std::clamp(sampleX, sourceMinX, sourceMaxX - 1);

			std::size_t sourceIndex = (static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(owner_.paint_buffer_width_) + static_cast<std::size_t>(sampleX)) * 4u;
			std::size_t targetIndex = (static_cast<std::size_t>(targetY) * static_cast<std::size_t>(targetWidth) + static_cast<std::size_t>(targetX)) * 4u;

			panelPixels[targetIndex + 0] = owner_.paint_buffer_[sourceIndex + 0];
			panelPixels[targetIndex + 1] = owner_.paint_buffer_[sourceIndex + 1];
			panelPixels[targetIndex + 2] = owner_.paint_buffer_[sourceIndex + 2];
			panelPixels[targetIndex + 3] = owner_.paint_buffer_[sourceIndex + 3];
		}
	}

	if (textureData.texture_id == 0)
	{
		glGenTextures(1, &textureData.texture_id);
		glBindTexture(GL_TEXTURE_2D, textureData.texture_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, textureData.texture_id);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, targetWidth, targetHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, panelPixels.data());

	textureData.width = targetWidth;
	textureData.height = targetHeight;
	textureData.dirty = false;

	outFrame.x = 0;
	outFrame.y = 0;
	outFrame.width = targetWidth;
	outFrame.height = targetHeight;
	return true;
}

bool CEF_Resizer::translateMousePosition(float inputX, float inputY, int& outputX, int& outputY)
{
	std::lock_guard<std::mutex> lock(owner_.render_mutex_);

	outputX = static_cast<int>(std::lround(inputX));
	outputY = static_cast<int>(std::lround(inputY));

	for (const auto& displayPair : owner_.ui_panel_display_frames_)
	{
		auto sourceIt = owner_.ui_panel_frames_.find(displayPair.first);
		if (sourceIt == owner_.ui_panel_frames_.end())
		{
			continue;
		}

		const CEF_Drawer::UIPanelFrameData& displayFrame = displayPair.second;
		const CEF_Drawer::UIPanelFrameData& sourceFrame = sourceIt->second;

		if (displayFrame.width <= 0 || displayFrame.height <= 0 || sourceFrame.width <= 0 || sourceFrame.height <= 0)
		{
			continue;
		}

		float minX = static_cast<float>(displayFrame.x);
		float minY = static_cast<float>(displayFrame.y);
		float maxX = minX + static_cast<float>(displayFrame.width);
		float maxY = minY + static_cast<float>(displayFrame.height);

		if (inputX < minX || inputX >= maxX || inputY < minY || inputY >= maxY)
		{
			continue;
		}

		float normalizedX = (inputX - minX) / static_cast<float>(displayFrame.width);
		float normalizedY = (inputY - minY) / static_cast<float>(displayFrame.height);

		normalizedX = std::clamp(normalizedX, 0.0f, 1.0f);
		normalizedY = std::clamp(normalizedY, 0.0f, 1.0f);

		float mappedX = static_cast<float>(sourceFrame.x) + normalizedX * static_cast<float>(sourceFrame.width);
		float mappedY = static_cast<float>(sourceFrame.y) + normalizedY * static_cast<float>(sourceFrame.height);

		outputX = static_cast<int>(std::lround(mappedX));
		outputY = static_cast<int>(std::lround(mappedY));
		return true;
	}

	return false;
}

void CEF_Resizer::flushRuntimeLayoutSync()
{
	std::map<std::string, CEF_Drawer::UIPanelFrameData> panelFrames;
	CefRefPtr<CefBrowser> browser;
	{
		std::lock_guard<std::mutex> lock(owner_.render_mutex_);
		owner_.runtime_layout_sync_pending_ = false;
		panelFrames = owner_.runtime_layout_frames_;
		browser = owner_.browser_;
	}

	if (!browser || panelFrames.empty())
	{
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame || !mainFrame->IsValid())
	{
		return;
	}

	const std::string script = buildRuntimeLayoutSyncScript(panelFrames);
	if (script.empty())
	{
		return;
	}

	mainFrame->ExecuteJavaScript(script, mainFrame->GetURL(), 0);

	CefRefPtr<CefBrowserHost> host = browser->GetHost();
	if (host)
	{
		host->Invalidate(PET_VIEW);
	}
}

std::string CEF_Resizer::buildRuntimeLayoutSyncScript(const std::map<std::string, CEF_Drawer::UIPanelFrameData>& panelFrames) const
{
	if (panelFrames.empty())
	{
		return std::string();
	}

	std::ostringstream script;
	script << "(function(){";
	script << "const frameMap={";
	bool firstFrame = true;
	for (const auto& pair : panelFrames)
	{
		if (!firstFrame)
		{
			script << ",";
		}

		firstFrame = false;
		script << "'" << pair.first << "':{";
		script << "x:" << pair.second.x << ",";
		script << "y:" << pair.second.y << ",";
		script << "width:" << pair.second.width << ",";
		script << "height:" << pair.second.height;
		script << "}";
	}
	script << "};";
	script
		<< "const container=document.querySelector('.main-container');"
		<< "const leftSection=document.querySelector('.left-section');"
		<< "const centerSection=document.querySelector('.center-section');"
		<< "const rightSection=document.querySelector('.right-section');"
		<< "const topRow=document.querySelector('.top-row');"
		<< "const projectViewerPanel=document.querySelector('.project-viewer-panel');"
		<< "if(!container||!leftSection||!centerSection||!rightSection||!topRow||!projectViewerPanel){return;}"
		<< "const sectionWidths={};"
		<< "let topRowTop=null;"
		<< "let topRowBottom=null;"
		<< "const getPanelName=function(iframe){"
		<< "if(!iframe||!iframe.parentElement){return null;}"
		<< "const panelClasses=iframe.parentElement.classList;"
		<< "for(let index=0;index<panelClasses.length;++index){"
		<< "const className=panelClasses[index];"
		<< "if(!className||className==='panel'){continue;}"
		<< "return className.replace(/-/g,'_');"
		<< "}"
		<< "return null;"
		<< "};"
		<< "container.querySelectorAll('iframe').forEach(function(iframe){"
		<< "const panelName=getPanelName(iframe);"
		<< "if(!panelName){return;}"
		<< "const frame=frameMap[panelName];"
		<< "if(!frame||frame.width<=0||frame.height<=0){return;}"
		<< "const panelElement=iframe.parentElement;"
		<< "const sectionElement=panelElement?panelElement.parentElement:null;"
		<< "if(!panelElement){return;}"
		<< "if(panelElement.classList.contains('project-viewer-panel')){"
		<< "panelElement.style.flex='0 0 auto';"
		<< "panelElement.style.height=frame.height+'px';"
		<< "return;"
		<< "}"
		<< "if(!sectionElement||!sectionElement.classList){return;}"
		<< "let sectionKey='';"
		<< "if(sectionElement.classList.contains('left-section')){sectionKey='left-section';}"
		<< "else if(sectionElement.classList.contains('center-section')){sectionKey='center-section';}"
		<< "else if(sectionElement.classList.contains('right-section')){sectionKey='right-section';}"
		<< "if(!sectionKey){return;}"
		<< "topRowTop=topRowTop===null?frame.y:Math.min(topRowTop,frame.y);"
		<< "topRowBottom=topRowBottom===null?(frame.y+frame.height):Math.max(topRowBottom,frame.y+frame.height);"
		<< "sectionElement.style.flex='0 0 auto';"
		<< "sectionWidths[sectionKey]=sectionWidths[sectionKey]?Math.max(sectionWidths[sectionKey],frame.width):frame.width;"
		<< "if(sectionKey==='right-section'){"
		<< "panelElement.style.flex='0 0 auto';"
		<< "panelElement.style.height=frame.height+'px';"
		<< "}else{"
		<< "panelElement.style.height='';"
		<< "panelElement.style.flex='';"
		<< "}"
		<< "});"
		<< "leftSection.style.flex='0 0 auto';"
		<< "centerSection.style.flex='0 0 auto';"
		<< "rightSection.style.flex='0 0 auto';"
		<< "projectViewerPanel.style.flex='0 0 auto';"
		<< "if(sectionWidths['left-section']){leftSection.style.width=sectionWidths['left-section']+'px';}"
		<< "if(sectionWidths['center-section']){centerSection.style.width=sectionWidths['center-section']+'px';}"
		<< "if(sectionWidths['right-section']){rightSection.style.width=sectionWidths['right-section']+'px';}"
		<< "if(topRowTop!==null&&topRowBottom!==null){topRow.style.flex='0 0 auto';topRow.style.height=(topRowBottom-topRowTop)+'px';}"
		<< "if(leftSection.firstElementChild){leftSection.firstElementChild.style.height='';leftSection.firstElementChild.style.flex='';}"
		<< "if(centerSection.firstElementChild){centerSection.firstElementChild.style.height='';centerSection.firstElementChild.style.flex='';}"
		<< "document.body.offsetHeight;"
		<< "})();";

	return script.str();
}

void CEF_Resizer::forceLayoutSync()
{
	if (!owner_.browser_)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(owner_.render_mutex_);
		owner_.runtime_layout_waiting_for_paint_ = true;
		for (auto& texturePair : owner_.ui_panel_textures_)
		{
			texturePair.second.dirty = true;
		}
	}

	CefRefPtr<CefBrowserHost> host = owner_.browser_->GetHost();
	if (host)
	{
		host->WasResized();
		host->Invalidate(PET_VIEW);
	}
}
