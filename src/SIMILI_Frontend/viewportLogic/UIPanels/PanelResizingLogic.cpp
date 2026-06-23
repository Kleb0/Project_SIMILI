#define NOMINMAX
#include "PanelResizingLogic.hpp"
#include "UIManager.hpp"
#include "../../SDL_ApplicationWindow.hpp"
#include "IFrameDatas.hpp"
#include "../../App_Border.hpp"
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

void PanelResizingLogic::setFrameDatas(SIMILI::Frontend::IFrameDatas* frameDatas)
{
	frame_datas_ = frameDatas;
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

void PanelResizingLogic::syncBrowserPanelLayoutFromCurrentFrames(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight, bool suppressServerNotify)
{
	// =========================================================================
	// PART 1: Layout Anchor Determination
	// Identify Left-Column, Right-Column, and Bottom panels from geometries
	// =========================================================================

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
		CefPostTask(TID_UI, base::BindOnce(&PanelResizingLogic::syncBrowserPanelLayoutFromCurrentFrames, base::Unretained(this), windowState, currentWidth, currentHeight, referenceWidth, referenceHeight, suppressServerNotify));
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

	PanelLayoutBoundary boundary = findColumnsAndBoundaries(iframeDataMap);
	if (boundary.bottomPanelName.empty())
	{
		std::cout << "[PanelResizingLogic] syncBrowserPanelLayoutFromCurrentFrames: skipped, no bottom panel found" << std::endl;
		return;
	}

	// =========================================================================
	// PART 2: Scale Factor and Sizing Calculation
	// Apply DPI and render states to compute final element widths and heights
	// =========================================================================

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

	PanelDimensions dims = calculatePanelDimensions(
		iframeDataMap, scaleX, scaleY, currentHeight, boundary.topRowHeight,
		boundary.leftTopName, boundary.leftBottomName, boundary.rightTopName, boundary.rightBottomName, boundary.bottomPanelName, 30);

	// =========================================================================
	// PART 3: JavaScript Block Construction & Execution
	// Generate clean, inline flexbox style overlays applied to DOM
	// =========================================================================

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
		<< "const isInitState=" << ((windowState == WindowRenderState::Init && !suppressServerNotify) ? "true" : "false") << ";"
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
		<< "const leftTopPanel=" << (dims.leftTopSelector.empty() ? "null" : "clampPanel('" + dims.leftTopSelector + "')") << ";"
		<< "const leftBottomPanel=" << (dims.leftBottomSelector.empty() ? "null" : "clampPanel('" + dims.leftBottomSelector + "')") << ";"
		<< "const rightTopPanel=" << (dims.rightTopSelector.empty() ? "null" : "clampPanel('" + dims.rightTopSelector + "')") << ";"
		<< "const rightBottomPanel=" << (dims.rightBottomSelector.empty() ? "null" : "clampPanel('" + dims.rightBottomSelector + "')") << ";"
		<< "const project=clampPanel('" << dims.bottomSelector << "');"
		<< buildFixedWidthBlock("left", dims.leftWidth)
		<< "if(center){center.style.flex='1 1 auto';center.style.minWidth='0';center.style.overflow='hidden';}"
		<< buildFixedWidthBlock("right", dims.rightWidth)
		<< buildFixedHeightBlock("topRow", dims.topRowHeightScaled)
		<< buildFixedPanelBlock("leftTopPanel", dims.leftWidth, dims.leftTopHeight)
		<< buildFixedPanelBlock("leftBottomPanel", dims.leftWidth, dims.leftBottomHeight)
		<< buildFixedPanelBlock("rightTopPanel", dims.rightWidth, dims.rightTopHeight)
		<< buildFixedPanelBlock("rightBottomPanel", dims.rightWidth, dims.rightBottomHeight)
		<< buildFixedHeightBlock("project", dims.bottomHeight)
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

	PanelLayoutBoundary boundary = findColumnsAndBoundaries(iframeDataMap);
	if (boundary.bottomPanelName.empty())
	{
		std::cout << "[PanelResizingLogic] syncBrowserFullScreenPanelLayout: skipped, no bottom panel found" << std::endl;
		return;
	}

	// For Fullscreen, layout frames are already at correct positions. No scaling required (use 1.0f)
	PanelDimensions dims = calculatePanelDimensions(
		iframeDataMap, 1.0f, 1.0f, currentHeight, boundary.topRowHeight,
		boundary.leftTopName, boundary.leftBottomName, boundary.rightTopName, boundary.rightBottomName, boundary.bottomPanelName, scaledTopBarHeight);

	{
		int wsX = dims.leftWidth;
		int wsY = scaledTopBarHeight;
		int wsW = (std::max)(1, currentWidth - dims.leftWidth - dims.rightWidth);
		int wsH = (std::max)(1, currentHeight - scaledTopBarHeight - dims.bottomHeight);
		Set_Workspace_Render_Borders(wsX, wsY, wsW, wsH);
	}

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
		<< "const leftTopPanel=" << (dims.leftTopSelector.empty() ? "null" : "clampPanel('" + dims.leftTopSelector + "')") << ";"
		<< "const leftBottomPanel=" << (dims.leftBottomSelector.empty() ? "null" : "clampPanel('" + dims.leftBottomSelector + "')") << ";"
		<< "const rightTopPanel=" << (dims.rightTopSelector.empty() ? "null" : "clampPanel('" + dims.rightTopSelector + "')") << ";"
		<< "const rightBottomPanel=" << (dims.rightBottomSelector.empty() ? "null" : "clampPanel('" + dims.rightBottomSelector + "')") << ";"
		<< "const project=clampPanel('" << dims.bottomSelector << "');"
		<< "if(left){left.style.width='" << dims.leftWidth << "px';left.style.minWidth='" << dims.leftWidth << "px';left.style.maxWidth='" << dims.leftWidth << "px';left.style.flex='0 0 " << dims.leftWidth << "px';left.style.overflow='hidden';}"
		<< "if(center){center.style.flex='1 1 auto';center.style.minWidth='0';center.style.overflow='hidden';}"
		<< "if(right){right.style.width='" << dims.rightWidth << "px';right.style.minWidth='" << dims.rightWidth << "px';right.style.maxWidth='" << dims.rightWidth << "px';right.style.flex='0 0 " << dims.rightWidth << "px';right.style.overflow='hidden';}"
		<< "if(topRow){topRow.style.height='" << dims.topRowHeightScaled << "px';topRow.style.minHeight='" << dims.topRowHeightScaled << "px';topRow.style.maxHeight='" << dims.topRowHeightScaled << "px';topRow.style.flex='0 0 " << dims.topRowHeightScaled << "px';topRow.style.overflow='hidden';}"
		<< "if(leftTopPanel){leftTopPanel.style.width='" << dims.leftWidth << "px';leftTopPanel.style.height='" << dims.leftTopHeight << "px';leftTopPanel.style.minWidth='" << dims.leftWidth << "px';leftTopPanel.style.minHeight='" << dims.leftTopHeight << "px';leftTopPanel.style.maxWidth='" << dims.leftWidth << "px';leftTopPanel.style.maxHeight='" << dims.leftTopHeight << "px';leftTopPanel.style.flex='0 0 auto';}"
		<< "if(leftBottomPanel){leftBottomPanel.style.width='" << dims.leftWidth << "px';leftBottomPanel.style.height='" << dims.leftBottomHeight << "px';leftBottomPanel.style.minWidth='" << dims.leftWidth << "px';leftBottomPanel.style.minHeight='" << dims.leftBottomHeight << "px';leftBottomPanel.style.maxWidth='" << dims.leftWidth << "px';leftBottomPanel.style.maxHeight='" << dims.leftBottomHeight << "px';leftBottomPanel.style.flex='0 0 auto';}"
		<< "if(rightTopPanel){rightTopPanel.style.width='" << dims.rightWidth << "px';rightTopPanel.style.height='" << dims.rightTopHeight << "px';rightTopPanel.style.minWidth='" << dims.rightWidth << "px';rightTopPanel.style.minHeight='" << dims.rightTopHeight << "px';rightTopPanel.style.maxWidth='" << dims.rightWidth << "px';rightTopPanel.style.maxHeight='" << dims.rightTopHeight << "px';rightTopPanel.style.flex='0 0 auto';}"
		<< "if(rightBottomPanel){rightBottomPanel.style.width='" << dims.rightWidth << "px';rightBottomPanel.style.height='" << dims.rightBottomHeight << "px';rightBottomPanel.style.minWidth='" << dims.rightWidth << "px';rightBottomPanel.style.minHeight='" << dims.rightBottomHeight << "px';rightBottomPanel.style.maxWidth='" << dims.rightWidth << "px';rightBottomPanel.style.maxHeight='" << dims.rightBottomHeight << "px';rightBottomPanel.style.flex='0 0 auto';}"
		<< "if(project){project.style.width='100%';project.style.minWidth='0';project.style.maxWidth='100%';project.style.alignSelf='stretch';project.style.height='" << dims.bottomHeight << "px';project.style.minHeight='" << dims.bottomHeight << "px';project.style.maxHeight='" << dims.bottomHeight << "px';project.style.flex='0 0 " << dims.bottomHeight << "px';}"
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

PanelResizingLogic::PanelDimensions PanelResizingLogic::calculatePanelDimensions(
	const std::map<std::string, IFrameData>& iframeDataMap,
	float scaleX,
	float scaleY,
	int currentHeight,
	int topRowHeight,
	const std::string& leftTopName,
	const std::string& leftBottomName,
	const std::string& rightTopName,
	const std::string& rightBottomName,
	const std::string& bottomPanelName,
	int topBarHeight) const
{
	auto selectorForPanel = [](const std::string& panelName)
	{
		if (panelName.empty()) return std::string();
		std::string selector = panelName;
		std::replace(selector.begin(), selector.end(), '_', '-');
		return selector;
	};

	PanelDimensions dims;

	dims.leftTopSelector = selectorForPanel(leftTopName);
	dims.leftBottomSelector = (leftBottomName != leftTopName) ? selectorForPanel(leftBottomName) : "";
	dims.rightTopSelector = selectorForPanel(rightTopName);
	dims.rightBottomSelector = (rightBottomName != rightTopName) ? selectorForPanel(rightBottomName) : "";
	dims.bottomSelector = selectorForPanel(bottomPanelName);

	// Left section dimensions
	if (!leftTopName.empty() && iframeDataMap.count(leftTopName))
	{
		dims.leftWidth = static_cast<int>(std::round(iframeDataMap.at(leftTopName).width * scaleX));
		dims.leftTopHeight = static_cast<int>(std::round(iframeDataMap.at(leftTopName).height * scaleY));
	}
	if (!dims.leftBottomSelector.empty() && iframeDataMap.count(leftBottomName))
	{
		dims.leftBottomHeight = static_cast<int>(std::round(iframeDataMap.at(leftBottomName).height * scaleY));
	}

	// Right section dimensions
	if (!rightTopName.empty() && iframeDataMap.count(rightTopName))
	{
		dims.rightWidth = static_cast<int>(std::round(iframeDataMap.at(rightTopName).width * scaleX));
		dims.rightTopHeight = static_cast<int>(std::round(iframeDataMap.at(rightTopName).height * scaleY));
	}
	if (!dims.rightBottomSelector.empty() && iframeDataMap.count(rightBottomName))
	{
		dims.rightBottomHeight = static_cast<int>(std::round(iframeDataMap.at(rightBottomName).height * scaleY));
	}

	// Global layouts heights
	dims.topRowHeightScaled = static_cast<int>(std::round(topRowHeight * scaleY));
	if (dims.topRowHeightScaled > 0)
	{
		dims.bottomHeight = (std::max)(1, currentHeight - topBarHeight - dims.topRowHeightScaled);
	}
	else if (!bottomPanelName.empty() && iframeDataMap.count(bottomPanelName))
	{
		dims.bottomHeight = static_cast<int>(std::round(iframeDataMap.at(bottomPanelName).height * scaleY));
	}

	return dims;
}

PanelResizingLogic::PanelLayoutBoundary PanelResizingLogic::findColumnsAndBoundaries(
const std::map<std::string, IFrameData>& iframeDataMap) const
{
	PanelLayoutBoundary boundary;
	boundary.minX = INT_MAX;
	boundary.maxX = INT_MIN;
	int maxBottomY = INT_MIN;

	for (const auto& pair : iframeDataMap)
	{
		const std::string& name = pair.first;
		const auto& data = pair.second;

		if (data.x < boundary.minX) boundary.minX = data.x;
		if (data.x > boundary.maxX) boundary.maxX = data.x;
		if (data.y > maxBottomY)
		{
			maxBottomY = data.y;
			boundary.bottomPanelName = name;
		}
	}

	if (boundary.bottomPanelName.empty())
	{
		return boundary;
	}

	int leftTopY = INT_MAX;
	int leftBottomY = INT_MIN;

	int rightTopY = INT_MAX;
	int rightBottomY = INT_MIN;

	int topRowYStart = INT_MAX;
	int topRowYEnd = INT_MIN;

	for (const auto& pair : iframeDataMap)
	{
		const std::string& name = pair.first;
		const auto& data = pair.second;

		if (name == boundary.bottomPanelName)
			continue;

		// Track overall top row height span
		if (data.y < topRowYStart) topRowYStart = data.y;
		if (data.y + data.height > topRowYEnd) topRowYEnd = data.y + data.height;

		// Left Column panels
		if (data.x == boundary.minX)
		{
			if (data.y < leftTopY) { leftTopY = data.y; boundary.leftTopName = name; }
			if (data.y > leftBottomY) { leftBottomY = data.y; boundary.leftBottomName = name; }
		}

		// Right Column panels
		if (data.x == boundary.maxX)
		{
			if (data.y < rightTopY) { rightTopY = data.y; boundary.rightTopName = name; }
			if (data.y > rightBottomY) { rightBottomY = data.y; boundary.rightBottomName = name; }
		}
	}

	// Fallbacks if no side panels exist
	if (boundary.leftTopName.empty()) boundary.leftTopName = boundary.leftBottomName;
	if (boundary.rightTopName.empty()) boundary.rightTopName = boundary.rightBottomName;

	// Calculate overall height of the top row (everything except bottom panel)
	boundary.topRowHeight = (topRowYStart != INT_MAX && topRowYEnd > topRowYStart) ? (topRowYEnd - topRowYStart) : 0;
	if (boundary.topRowHeight <= 0 && !boundary.leftTopName.empty() && iframeDataMap.count(boundary.leftTopName))
	{
		boundary.topRowHeight = iframeDataMap.at(boundary.leftTopName).height;
	}

	return boundary;
}

void PanelResizingLogic::Set_App_Borders(App_Border& App_Borders, SDL_Window* window, int borderLeft, int borderTop, int borderWidth, int borderHeight)
{
	border_left_ = borderLeft;
	border_top_ = borderTop;
	border_width_ = borderWidth;
	border_height_ = borderHeight;

	std::cout << "[PanelResizingLogic] : App Borders dimensions are " << " width : " << borderWidth << " | height : " << borderHeight  << std::endl;

}

void PanelResizingLogic::Set_Workspace_Render_Borders(int x, int y, int width, int height)
{
	workspace_x_ = x;
	workspace_y_ = y;
	workspace_width_ = width;
	workspace_height_ = height;

	if (ui_manager_)
	{
		ui_manager_->setWorkSpace(x, y, width, height);
	}
}

void PanelResizingLogic::Redraw_Inside_App_Borders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, bool skipTextureRebuild, SDL_Window* window, int referenceWidth, int referenceHeight)
{
	if (!ui_manager_)
	{
		return;
	}

	ui_manager_->setBorders(border_left_, border_top_, border_width_, border_height_);

	if (ui_manager_->isWindowMaximized())
	{
		ui_manager_->renderFullScreenUIPanelsInsideBorders(commandBuffer, drawableWidth, drawableHeight, skipTextureRebuild, window);
	}
	else
	{
		auto panelFrameDataMap = ui_manager_->getUIPanelFrameDatas();
		ui_manager_->drawReduceScreenUIpanelsInsideBorders(commandBuffer, drawableWidth, drawableHeight,
			panelFrameDataMap, skipTextureRebuild, window, referenceWidth, referenceHeight);
	}
}
