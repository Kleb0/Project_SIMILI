#define NOMINMAX
#include "PanelResizingLogic.hpp"
#include "UIManager.hpp"
#include "FrameDataCatcher.hpp"
#include "../../SDL_ApplicationWindow.hpp"
#include "FrameDatas.hpp"
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/cef_task.h"
#include <algorithm>
#include <sstream>
#include <iostream>

PanelResizingLogic::PanelResizingLogic()
	: ui_manager_(nullptr)
	, sdl_window_(nullptr)
	, frame_datas_(nullptr)
	, frame_data_catcher_(nullptr)
	, pending_deferred_layout_refresh_(false)
{
}

void PanelResizingLogic::setUIManager(SIMILI::Frontend::UIManager* uiManager)
{
	ui_manager_ = uiManager;
}

void PanelResizingLogic::setSDLWindow(SDL_ApplicationWindow* sdlWindow)
{
	sdl_window_ = sdlWindow;
}

void PanelResizingLogic::setFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas)
{
	frame_datas_ = frameDatas;
}

void PanelResizingLogic::setFrameDataCatcher(FrameDataCatcher* catcher)
{
	frame_data_catcher_ = catcher;
}

void PanelResizingLogic::cacheUIPanelFrameDatas()
{
	if (!ui_manager_)
	{
		return;
	}

	auto iframeDataMap = ui_manager_->getResolvedUIPanelIFrames();
	if (iframeDataMap.empty())
	{
		std::cout << "[PanelResizingLogic] cacheUIPanelFrameDatas: No panel frame data from UIManager" << std::endl;
		return;
	}

	std::map<std::string, IFrameData> currentIFrames;
	if (frame_data_catcher_)
	{
		currentIFrames = frame_data_catcher_->getAllIFrames();
	}

	bool hasPanelLayoutChange = false;

	for (const auto& pair : iframeDataMap)
	{
		auto currentIt = currentIFrames.find(pair.first);
		if (currentIt == currentIFrames.end() ||
			currentIt->second.x != pair.second.x ||
			currentIt->second.y != pair.second.y ||
			currentIt->second.width != pair.second.width ||
			currentIt->second.height != pair.second.height ||
			currentIt->second.clientX != pair.second.clientX ||
			currentIt->second.clientY != pair.second.clientY)
		{
			hasPanelLayoutChange = true;
			if (frame_data_catcher_)
			{
				frame_data_catcher_->iframe_data_map_[pair.first] = pair.second;
			}
		}
	}

	ui_manager_->updateUIPanelIFrames(iframeDataMap);

	if (hasPanelLayoutChange && frame_datas_ && sdl_window_)
	{
		frame_datas_->catchFrameData(sdl_window_->getHandle());
		ui_manager_->cacheUIPanelFrameDatas(frame_datas_, {});
		std::cout << "[PanelResizingLogic] cacheUIPanelFrameDatas: Refreshed FrameDatas with "
			      << iframeDataMap.size() << " splitter-adjusted UI panels" << std::endl;
	}
	else
	{
		std::cout << "[PanelResizingLogic] cacheUIPanelFrameDatas: layout already up to date ("
			      << iframeDataMap.size() << " panels)" << std::endl;
	}
}

void PanelResizingLogic::syncBrowserPanelLayoutFromCurrentFrames(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight)
{
	if (!ui_manager_)
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, ui_manager_ is null" << std::endl;
		return;
	}

	auto iframeDataMap = ui_manager_->getResolvedUIPanelIFrames();
	iframeDataMap.erase("top_bar_panel");
	if (iframeDataMap.empty())
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, no resolved UI panel frames" << std::endl;
		return;
	}

	if (!CefCurrentlyOn(TID_UI))
	{
		CefPostTask(TID_UI, base::BindOnce(&PanelResizingLogic::syncBrowserPanelLayoutFromCurrentFrames, base::Unretained(this), windowState, currentWidth, currentHeight, referenceWidth, referenceHeight));
		return;
	}

	if (!sdl_window_)
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, sdl_window_ is null" << std::endl;
		return;
	}

	CefRefPtr<CefBrowser> browser = sdl_window_->getBrowser();
	if (!browser)
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, browser is null" << std::endl;
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame || !mainFrame->IsValid())
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, main frame is invalid" << std::endl;
		return;
	}

	const auto leftColumnIt = std::min_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x < rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto rightColumnIt = std::min_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x > rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto bottomPanelIt = std::max_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.width != rhs.second.width)
			{
				return lhs.second.width < rhs.second.width;
			}
			return lhs.second.y < rhs.second.y;
		});

	if (leftColumnIt == iframeDataMap.end() || rightColumnIt == iframeDataMap.end() || bottomPanelIt == iframeDataMap.end())
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, unable to derive layout anchors" << std::endl;
		return;
	}

	int topRowHeight = 0;
	for (const auto& pair : iframeDataMap)
	{
		if (pair.first == bottomPanelIt->first)
		{
			continue;
		}
		topRowHeight = (std::max)(topRowHeight, pair.second.height);
	}

	if (topRowHeight <= 0)
	{
		topRowHeight = leftColumnIt->second.height;
	}

	auto rightTopPanelIt = iframeDataMap.end();
	auto rightBottomPanelIt = iframeDataMap.end();
	for (auto it = iframeDataMap.begin(); it != iframeDataMap.end(); ++it)
	{
		if (it->first == bottomPanelIt->first)
		{
			continue;
		}

		if (it->second.x != rightColumnIt->second.x)
		{
			continue;
		}

		if (rightTopPanelIt == iframeDataMap.end() || it->second.y < rightTopPanelIt->second.y)
		{
			rightTopPanelIt = it;
		}

		if (rightBottomPanelIt == iframeDataMap.end() || it->second.y > rightBottomPanelIt->second.y)
		{
			rightBottomPanelIt = it;
		}
	}

	float scaleX = 1.0f;
	float scaleY = 1.0f;

	if (windowState == WindowRenderState::Maximized || windowState == WindowRenderState::Reduced)
	{
		if (referenceWidth > 0 && referenceHeight > 0 && currentWidth > 0 && currentHeight > 0)
		{
			scaleX = static_cast<float>(currentWidth) / static_cast<float>(referenceWidth);
			scaleY = static_cast<float>(currentHeight) / static_cast<float>(referenceHeight);
		}
	}

	auto selectorForPanel = [](const std::string& panelName)
	{
		std::string selector = panelName;
		std::replace(selector.begin(), selector.end(), '_', '-');
		return selector;
	};

	const std::string leftPanelSelector = selectorForPanel(leftColumnIt->first);
	const std::string bottomPanelSelector = selectorForPanel(bottomPanelIt->first);
	const std::string rightTopPanelSelector = (rightTopPanelIt != iframeDataMap.end()) ? selectorForPanel(rightTopPanelIt->first) : std::string();
	const std::string rightBottomPanelSelector = (rightBottomPanelIt != iframeDataMap.end()) ? selectorForPanel(rightBottomPanelIt->first) : std::string();
	const int leftWidth = leftColumnIt->second.width;
	const int leftHeight = leftColumnIt->second.height;
	const int rightWidth = rightColumnIt->second.width;
	const int rightTopHeight = (rightTopPanelIt != iframeDataMap.end()) ? rightTopPanelIt->second.height : 0;
	const int rightBottomHeight = (rightBottomPanelIt != iframeDataMap.end())
		? ((rightTopPanelIt == iframeDataMap.end() || rightBottomPanelIt->first == rightTopPanelIt->first)
			? topRowHeight
			: std::max(1, topRowHeight - rightTopHeight))
		: 0;
	const int bottomHeight = (topRowHeight > 0)
		? std::max(1, currentHeight - 30 - topRowHeight)
		: bottomPanelIt->second.height;

	auto buildFixedWidthBlock = [](const char* elementName, int width)
	{
		std::ostringstream block;
		block << "if(" << elementName << "){"
			<< elementName << ".style.width='" << width << "px';"
			<< elementName << ".style.minWidth='" << width << "px';"
			<< elementName << ".style.maxWidth='" << width << "px';"
			<< elementName << ".style.flex='0 0 " << width << "px';"
			<< elementName << ".style.overflow='hidden';}";
		return block.str();
	};

	auto buildFixedHeightBlock = [](const char* elementName, int height)
	{
		std::ostringstream block;
		block << "if(" << elementName << "){"
			<< elementName << ".style.height='" << height << "px';"
			<< elementName << ".style.minHeight='" << height << "px';"
			<< elementName << ".style.maxHeight='" << height << "px';"
			<< elementName << ".style.flex='0 0 " << height << "px';"
			<< elementName << ".style.overflow='hidden';}";
		return block.str();
	};

	auto buildFixedPanelBlock = [](const char* elementName, int width, int height)
	{
		std::ostringstream block;
		block << "if(" << elementName << "){"
			<< elementName << ".style.width='" << width << "px';"
			<< elementName << ".style.height='" << height << "px';"
			<< elementName << ".style.minWidth='" << width << "px';"
			<< elementName << ".style.minHeight='" << height << "px';"
			<< elementName << ".style.maxWidth='" << width << "px';"
			<< elementName << ".style.maxHeight='" << height << "px';"
			<< elementName << ".style.flex='0 0 auto';}";
		return block.str();
	};

	std::ostringstream script;
	script
		<< "(function(){"
		<< "const currentWidth=" << currentWidth << ";"
		<< "const currentHeight=" << currentHeight << ";"
		<< "const referenceWidth=" << referenceWidth << ";"
		<< "const referenceHeight=" << referenceHeight << ";"
		<< "const layoutScaleX=" << scaleX << ";"
		<< "const layoutScaleY=" << scaleY << ";"
		<< "const isInitState=" << (windowState == WindowRenderState::Init ? "true" : "false") << ";"
		<< "const findPanelElement=function(selector){"
		<< "return document.querySelector('.'+selector) || document.querySelector('.'+selector.replace(/-UI/g,'_UI'));"
		<< "};"
		<< "const resetBox=function(element){"
		<< "if(!element){return null;}"
		<< "element.style.width='';"
		<< "element.style.height='';"
		<< "element.style.flex='';"
		<< "element.style.minWidth='';"
		<< "element.style.minHeight='';"
		<< "element.style.maxWidth='';"
		<< "element.style.maxHeight='';"
		<< "element.style.alignSelf='';"
		<< "return element;"
		<< "};"
		<< "const clampPanel=function(selector){"
		<< "const element=resetBox(findPanelElement(selector));"
		<< "if(!element){return null;}"
		<< "element.style.overflow='hidden';"
		<< "element.style.margin='0';"
		<< "element.style.boxSizing='border-box';"
		<< "return element;"
		<< "};"
		<< "const root=document.documentElement;"
		<< "const body=document.body;"
		<< "const mainContainer=document.querySelector('.main-container');"
		<< "if(root){root.style.width=currentWidth+'px';root.style.height=currentHeight+'px';root.style.minWidth=currentWidth+'px';root.style.minHeight=currentHeight+'px';root.style.maxWidth=currentWidth+'px';root.style.maxHeight=currentHeight+'px';root.style.overflow='hidden';}"
		<< "if(body){body.style.width=currentWidth+'px';body.style.height=currentHeight+'px';body.style.minWidth=currentWidth+'px';body.style.minHeight=currentHeight+'px';body.style.maxWidth=currentWidth+'px';body.style.maxHeight=currentHeight+'px';body.style.margin='0';body.style.overflow='hidden';}"
		<< "if(mainContainer){mainContainer.style.width=currentWidth+'px';mainContainer.style.height=currentHeight+'px';mainContainer.style.minWidth=currentWidth+'px';mainContainer.style.minHeight=currentHeight+'px';mainContainer.style.maxWidth=currentWidth+'px';mainContainer.style.maxHeight=currentHeight+'px';mainContainer.style.boxSizing='border-box';mainContainer.dataset.currentWidth=String(currentWidth);mainContainer.dataset.currentHeight=String(currentHeight);mainContainer.dataset.referenceWidth=String(referenceWidth);mainContainer.dataset.referenceHeight=String(referenceHeight);mainContainer.dataset.scaleX=String(layoutScaleX);mainContainer.dataset.scaleY=String(layoutScaleY);}"
		<< "const topBarPanelInit=document.querySelector('.top-bar-panel');"
		<< "if(topBarPanelInit){topBarPanelInit.style.height='30px';topBarPanelInit.style.minHeight='30px';topBarPanelInit.style.maxHeight='30px';topBarPanelInit.style.flex='0 0 30px';}"
		<< "const left=resetBox(document.querySelector('.left-section'));"
		<< "const center=resetBox(document.querySelector('.center-section'));"
		<< "const right=resetBox(document.querySelector('.right-section'));"
		<< "const topRow=resetBox(document.querySelector('.top-row'));"
		<< "const leftPanel=clampPanel('" << leftPanelSelector << "');"
		<< "const rightTopPanel=" << (rightTopPanelSelector.empty() ? "null" : "clampPanel('" + rightTopPanelSelector + "')") << ";"
		<< "const rightBottomPanel=" << (rightBottomPanelSelector.empty() ? "null" : "clampPanel('" + rightBottomPanelSelector + "')") << ";"
		<< "const project=clampPanel('" << bottomPanelSelector << "');"
		<< buildFixedWidthBlock("left", leftWidth)
		<< "if(center){center.style.flex='1 1 auto';center.style.minWidth='0';center.style.overflow='hidden';}"
		<< buildFixedWidthBlock("right", rightWidth)
		<< buildFixedHeightBlock("topRow", topRowHeight)
		<< buildFixedPanelBlock("leftPanel", leftWidth, leftHeight)
		<< buildFixedPanelBlock("rightTopPanel", rightWidth, rightTopHeight)
		<< buildFixedPanelBlock("rightBottomPanel", rightWidth, rightBottomHeight)
		<< buildFixedHeightBlock("project", bottomHeight)
		<< "document.body.offsetHeight;"
		<< "window.requestAnimationFrame(function(){"
		<< "if(window.notifyViewportResize){window.notifyViewportResize();}"
		<< "if(isInitState){"
		<< "if(window.sendUIPanelIFramesToServer){window.sendUIPanelIFramesToServer();}"
		<< "if(window.sendIFrameSizesToServer){window.sendIFrameSizesToServer();}"
		<< "}"
		<< "document.body.offsetHeight;"
		<< "});"
		<< "})();";

	mainFrame->ExecuteJavaScript(script.str(), mainFrame->GetURL(), 0);
	browser->GetHost()->Invalidate(PET_VIEW);

	CefPostDelayedTask(TID_UI, base::BindOnce(&PanelResizingLogic::finalizeDeferredLayoutRefresh, base::Unretained(this), browser), 32);

	std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: Applied layout to CEF DOM for "
		      << iframeDataMap.size() << " panels" << std::endl;
}

void PanelResizingLogic::syncBrowserFullScreenPanelLayout(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight)
{
	if (!ui_manager_)
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, ui_manager_ is null" << std::endl;
		return;
	}

	auto iframeDataMap = ui_manager_->getResolvedUIPanelIFrames();

	int scaledTopBarHeight = 30;
	{
		auto topBarIt = iframeDataMap.find("top_bar_panel");
		if (topBarIt != iframeDataMap.end() && topBarIt->second.height > 0)
		{
			scaledTopBarHeight = topBarIt->second.height;
		}
	}

	iframeDataMap.erase("top_bar_panel");

	if (iframeDataMap.empty())
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, no resolved UI panel frames" << std::endl;
		return;
	}

	if (!CefCurrentlyOn(TID_UI))
	{
		CefPostTask(TID_UI, base::BindOnce(&PanelResizingLogic::syncBrowserFullScreenPanelLayout, base::Unretained(this), windowState, currentWidth, currentHeight, referenceWidth, referenceHeight));
		return;
	}

	if (!sdl_window_)
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, sdl_window_ is null" << std::endl;
		return;
	}

	CefRefPtr<CefBrowser> browser = sdl_window_->getBrowser();
	if (!browser)
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, browser is null" << std::endl;
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame || !mainFrame->IsValid())
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, main frame is invalid" << std::endl;
		return;
	}

	auto selectorForPanel = [](const std::string& panelName)
	{
		std::string selector = panelName;
		std::replace(selector.begin(), selector.end(), '_', '-');
		return selector;
	};

	const auto leftColumnIt = std::min_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x < rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto rightColumnIt = std::max_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x < rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto bottomPanelIt = std::max_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.width != rhs.second.width)
			{
				return lhs.second.width < rhs.second.width;
			}
			return lhs.second.x < rhs.second.x;
		});

	if (leftColumnIt == iframeDataMap.end() || rightColumnIt == iframeDataMap.end() || bottomPanelIt == iframeDataMap.end())
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, unable to derive layout anchors" << std::endl;
		return;
	}

	int topRowHeight = 0;
	for (const auto& pair : iframeDataMap)
	{
		if (pair.first == bottomPanelIt->first)
		{
			continue;
		}
		topRowHeight = std::max(topRowHeight, pair.second.height);
	}

	if (topRowHeight <= 0)
	{
		topRowHeight = leftColumnIt->second.height;
	}

	auto rightTopPanelIt = iframeDataMap.end();
	auto rightBottomPanelIt = iframeDataMap.end();

	for (auto it = iframeDataMap.begin(); it != iframeDataMap.end(); ++it)
	{
		if (it->first == bottomPanelIt->first || it->first == leftColumnIt->first)
		{
			continue;
		}

		if (it->second.x != rightColumnIt->second.x)
		{
			continue;
		}

		if (rightTopPanelIt == iframeDataMap.end() || it->second.y < rightTopPanelIt->second.y)
		{
			rightTopPanelIt = it;
		}

		if (rightBottomPanelIt == iframeDataMap.end() || it->second.y > rightBottomPanelIt->second.y)
		{
			rightBottomPanelIt = it;
		}
	}

	const std::string leftPanelSelector = selectorForPanel(leftColumnIt->first);
	const std::string bottomPanelSelector = selectorForPanel(bottomPanelIt->first);
	const std::string rightTopPanelSelector = (rightTopPanelIt != iframeDataMap.end()) ? selectorForPanel(rightTopPanelIt->first) : std::string();
	const std::string rightBottomPanelSelector = (rightBottomPanelIt != iframeDataMap.end()) ? selectorForPanel(rightBottomPanelIt->first) : std::string();

	const int fsLeftW = leftColumnIt->second.width;
	const int fsLeftH = leftColumnIt->second.height;
	const int fsRightW = rightColumnIt->second.width;
	const int fsTopRowH = topRowHeight;
	const int fsBottomH = (topRowHeight > 0)
		? std::max(1, currentHeight - scaledTopBarHeight - topRowHeight)
		: bottomPanelIt->second.height;
	const int fsRightTopH = (rightTopPanelIt != iframeDataMap.end()) ? rightTopPanelIt->second.height : 0;
	const int fsRightBottomH = (rightBottomPanelIt != iframeDataMap.end())
		? ((rightTopPanelIt == iframeDataMap.end() || rightBottomPanelIt->first == rightTopPanelIt->first)
			? fsTopRowH
			: std::max(1, fsTopRowH - fsRightTopH))
		: 0;

	std::ostringstream script;
	script
		<< "(function(){"
		<< "const currentWidth=" << currentWidth << ";"
		<< "const currentHeight=" << currentHeight << ";"
		<< "const findPanelElement=function(selector){"
		<< "return document.querySelector('.'+selector) || document.querySelector('.'+selector.replace(/-UI/g,'_UI'));"
		<< "};"
		<< "const resetBox=function(element){"
		<< "if(!element){return null;}"
		<< "element.style.width='';"
		<< "element.style.height='';"
		<< "element.style.flex='';"
		<< "element.style.minWidth='';"
		<< "element.style.minHeight='';"
		<< "element.style.maxWidth='';"
		<< "element.style.maxHeight='';"
		<< "element.style.alignSelf='';"
		<< "return element;"
		<< "};"
		<< "const clampPanel=function(selector){"
		<< "const element=resetBox(findPanelElement(selector));"
		<< "if(!element){return null;}"
		<< "element.style.overflow='hidden';"
		<< "element.style.margin='0';"
		<< "element.style.boxSizing='border-box';"
		<< "return element;"
		<< "};"
		<< "const root=document.documentElement;"
		<< "const body=document.body;"
		<< "const mainContainer=document.querySelector('.main-container');"
		<< "if(root){root.style.width=currentWidth+'px';root.style.height=currentHeight+'px';root.style.minWidth=currentWidth+'px';root.style.minHeight=currentHeight+'px';root.style.maxWidth=currentWidth+'px';root.style.maxHeight=currentHeight+'px';root.style.overflow='hidden';}"
		<< "if(body){body.style.width=currentWidth+'px';body.style.height=currentHeight+'px';body.style.minWidth=currentWidth+'px';body.style.minHeight=currentHeight+'px';body.style.maxWidth=currentWidth+'px';body.style.maxHeight=currentHeight+'px';body.style.margin='0';body.style.overflow='hidden';}"
		<< "if(mainContainer){mainContainer.style.width=currentWidth+'px';mainContainer.style.height=currentHeight+'px';mainContainer.style.minWidth=currentWidth+'px';mainContainer.style.minHeight=currentHeight+'px';mainContainer.style.maxWidth=currentWidth+'px';mainContainer.style.maxHeight=currentHeight+'px';mainContainer.style.boxSizing='border-box';}"
		<< "const topBarPanel=document.querySelector('.top-bar-panel');"
		<< "if(topBarPanel){topBarPanel.style.height='" << scaledTopBarHeight << "px';topBarPanel.style.minHeight='" << scaledTopBarHeight << "px';topBarPanel.style.maxHeight='" << scaledTopBarHeight << "px';topBarPanel.style.flex='0 0 " << scaledTopBarHeight << "px';}"
		<< "const left=resetBox(document.querySelector('.left-section'));"
		<< "const center=resetBox(document.querySelector('.center-section'));"
		<< "const right=resetBox(document.querySelector('.right-section'));"
		<< "const topRow=resetBox(document.querySelector('.top-row'));"
		<< "const leftPanel=clampPanel('" << leftPanelSelector << "');"
		<< "const rightTopPanel=" << (rightTopPanelSelector.empty() ? "null" : "clampPanel('" + rightTopPanelSelector + "')") << ";"
		<< "const rightBottomPanel=" << (rightBottomPanelSelector.empty() ? "null" : "clampPanel('" + rightBottomPanelSelector + "')") << ";"
		<< "const project=clampPanel('" << bottomPanelSelector << "');"
		<< "if(left){left.style.width='" << fsLeftW << "px';left.style.minWidth='" << fsLeftW << "px';left.style.maxWidth='" << fsLeftW << "px';left.style.flex='0 0 " << fsLeftW << "px';left.style.overflow='hidden';}"
		<< "if(center){center.style.flex='1 1 auto';center.style.minWidth='0';center.style.overflow='hidden';}"
		<< "if(right){right.style.width='" << fsRightW << "px';right.style.minWidth='" << fsRightW << "px';right.style.maxWidth='" << fsRightW << "px';right.style.flex='0 0 " << fsRightW << "px';right.style.overflow='hidden';}"
		<< "if(topRow){topRow.style.height='" << fsTopRowH << "px';topRow.style.minHeight='" << fsTopRowH << "px';topRow.style.maxHeight='" << fsTopRowH << "px';topRow.style.flex='0 0 " << fsTopRowH << "px';topRow.style.overflow='hidden';}"
		<< "if(leftPanel){leftPanel.style.width='" << fsLeftW << "px';leftPanel.style.height='" << fsLeftH << "px';leftPanel.style.minWidth='" << fsLeftW << "px';leftPanel.style.minHeight='" << fsLeftH << "px';leftPanel.style.maxWidth='" << fsLeftW << "px';leftPanel.style.maxHeight='" << fsLeftH << "px';leftPanel.style.flex='0 0 auto';}"
		<< "if(rightTopPanel){rightTopPanel.style.width='" << fsRightW << "px';rightTopPanel.style.height='" << fsRightTopH << "px';rightTopPanel.style.minWidth='" << fsRightW << "px';rightTopPanel.style.minHeight='" << fsRightTopH << "px';rightTopPanel.style.maxWidth='" << fsRightW << "px';rightTopPanel.style.maxHeight='" << fsRightTopH << "px';rightTopPanel.style.flex='0 0 auto';}"
		<< "if(rightBottomPanel){rightBottomPanel.style.width='" << fsRightW << "px';rightBottomPanel.style.height='" << fsRightBottomH << "px';rightBottomPanel.style.minWidth='" << fsRightW << "px';rightBottomPanel.style.minHeight='" << fsRightBottomH << "px';rightBottomPanel.style.maxWidth='" << fsRightW << "px';rightBottomPanel.style.maxHeight='" << fsRightBottomH << "px';rightBottomPanel.style.flex='0 0 auto';}"
		<< "if(project){project.style.width='100%';project.style.minWidth='0';project.style.maxWidth='100%';project.style.alignSelf='stretch';project.style.height='" << fsBottomH << "px';project.style.minHeight='" << fsBottomH << "px';project.style.maxHeight='" << fsBottomH << "px';project.style.flex='0 0 " << fsBottomH << "px';}"
		<< "})();";

	mainFrame->ExecuteJavaScript(script.str(), mainFrame->GetURL(), 0);
	browser->GetHost()->Invalidate(PET_VIEW);

	CefPostDelayedTask(TID_UI, base::BindOnce(&PanelResizingLogic::finalizeDeferredLayoutRefresh, base::Unretained(this), browser), 32);

	std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: Applied fullscreen layout to CEF DOM for "
		      << iframeDataMap.size() << " panels" << std::endl;
}

void PanelResizingLogic::finalizeDeferredLayoutRefresh(CefRefPtr<CefBrowser> delayedBrowser)
{
	pending_deferred_layout_refresh_.store(true);

	if (delayedBrowser && delayedBrowser->GetHost())
	{
		delayedBrowser->GetHost()->Invalidate(PET_VIEW);
	}
}
