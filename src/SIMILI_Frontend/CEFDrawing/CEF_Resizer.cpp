#include "CEF_Resizer.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstring>

#include "include/cef_browser.h"
#include "../../Engine/VulkanScene/VKcontext.hpp"

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
	
	owner_.forceRepaint();
	
	if (owner_.browser_)
	{
		CefRefPtr<CefBrowserHost> host = owner_.browser_->GetHost();
		if (host)
		{
			host->WasResized();
		}
	}
	
	std::cout << "[CEF_Drawer] Resized (logical pixels): " << width << "x" << height << std::endl;
}

void CEF_Resizer::ensureTextureStorage(int width, int height)
{
	if (owner_.texture_image_ == VK_NULL_HANDLE || width <= 0 || height <= 0)
	{
		return;
	}

	VkDevice device = owner_.vk_context_->getDevice();
	
	vkDeviceWaitIdle(device);

	if (owner_.texture_view_ != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, owner_.texture_view_, nullptr);
		owner_.texture_view_ = VK_NULL_HANDLE;
	}

	if (owner_.texture_image_ != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, owner_.texture_image_, nullptr);
		owner_.texture_image_ = VK_NULL_HANDLE;
	}

	if (owner_.texture_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, owner_.texture_memory_, nullptr);
		owner_.texture_memory_ = VK_NULL_HANDLE;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &owner_.texture_image_) != VK_SUCCESS)
	{
		return;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, owner_.texture_image_, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = owner_.findMemoryType(memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &owner_.texture_memory_) != VK_SUCCESS)
	{
		return;
	}

	vkBindImageMemory(device, owner_.texture_image_, owner_.texture_memory_, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = owner_.texture_image_;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(device, &viewInfo, nullptr, &owner_.texture_view_);
	
	// Reset layout flag since we just created a new texture
	owner_.texture_layout_initialized_ = false;
}

bool CEF_Resizer::rebuildUIPanelTextureLocked(const std::string& panelName, CEF_Drawer::UIPanelTextureData& textureData, CEF_Drawer::UIPanelFrameData& outFrame)
{
	auto sourceIt = owner_.ui_panel_frames_.find(panelName);
	if (sourceIt == owner_.ui_panel_frames_.end())
	{
		std::cout << "[CEF_Resizer::rebuildUIPanelTextureLocked] " << panelName << " - Not found in ui_panel_frames_" << std::endl;
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
		std::cout << "[CEF_Resizer::rebuildUIPanelTextureLocked] " << panelName << " - Invalid dimensions: source " << sourceFrame.width << "x" << sourceFrame.height << ", display " << displayFrame.width << "x" << displayFrame.height << std::endl;
		return false;
	}

	if (owner_.paint_buffer_.empty() || owner_.paint_buffer_width_ <= 0 || owner_.paint_buffer_height_ <= 0)
	{
		std::cout << "[CEF_Resizer::rebuildUIPanelTextureLocked] " << panelName << " - Paint buffer not ready: " << owner_.paint_buffer_.size() << " bytes, " << owner_.paint_buffer_width_ << "x" << owner_.paint_buffer_height_ << std::endl;
		return false;
	}
	
	std::size_t expectedBufferSize = static_cast<std::size_t>(owner_.paint_buffer_width_) * static_cast<std::size_t>(owner_.paint_buffer_height_) * 4u;
	if (owner_.paint_buffer_.size() != expectedBufferSize)
	{
		std::cout << "[CEF_Resizer::rebuildUIPanelTextureLocked] " << panelName 
		          << " - Buffer size mismatch: got " << owner_.paint_buffer_.size() 
		          << " bytes, expected " << expectedBufferSize 
		          << " (" << owner_.paint_buffer_width_ << "x" << owner_.paint_buffer_height_ << "x4)" << std::endl;
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

	bool needsRebuild = textureData.dirty || textureData.texture_image == VK_NULL_HANDLE || textureData.width != targetWidth || textureData.height != targetHeight;
	if (!needsRebuild)
	{
		outFrame.x = 0;
		outFrame.y = 0;
		outFrame.width = textureData.width;
		outFrame.height = textureData.height;
		return true;
	}

	if (textureData.texture_image != VK_NULL_HANDLE && (textureData.width != targetWidth || textureData.height != targetHeight))
	{
		VkDevice device = owner_.vk_context_->getDevice();
		
		vkDeviceWaitIdle(device);

		if (textureData.texture_view != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, textureData.texture_view, nullptr);
			textureData.texture_view = VK_NULL_HANDLE;
		}

		if (textureData.texture_memory != VK_NULL_HANDLE)
		{
			vkFreeMemory(device, textureData.texture_memory, nullptr);
			textureData.texture_memory = VK_NULL_HANDLE;
		}

		if (textureData.texture_image != VK_NULL_HANDLE)
		{
			vkDestroyImage(device, textureData.texture_image, nullptr);
			textureData.texture_image = VK_NULL_HANDLE;
		}

		textureData.width = 0;
		textureData.height = 0;
		textureData.texture_layout_initialized = false;
	}

	std::cout << "[CEF_Resizer::rebuildUIPanelTextureLocked] " << panelName 
	          << " - Sampling from paint buffer: sourceRegion[" << sourceMinX << "," << sourceMinY 
	          << " " << sourceWidth << "x" << sourceHeight << "] -> target " << targetWidth << "x" << targetHeight << std::endl;

	std::vector<unsigned char> panelPixels(static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight) * 4u);
	
	std::size_t paintBufferSize = owner_.paint_buffer_.size();

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

			if (sourceIndex + 3 >= paintBufferSize)
			{
				std::cout << "[CEF_Resizer::rebuildUIPanelTextureLocked] " << panelName 
				          << " - OUT OF BOUNDS: sourceIndex=" << sourceIndex 
				          << " paintBufferSize=" << paintBufferSize
				          << " sampleX=" << sampleX << " sampleY=" << sampleY
				          << " paint_buffer_width=" << owner_.paint_buffer_width_ << std::endl;
				panelPixels[targetIndex + 0] = 0;
				panelPixels[targetIndex + 1] = 0;
				panelPixels[targetIndex + 2] = 0;
				panelPixels[targetIndex + 3] = 255;
				continue;
			}

			panelPixels[targetIndex + 0] = owner_.paint_buffer_[sourceIndex + 0];
			panelPixels[targetIndex + 1] = owner_.paint_buffer_[sourceIndex + 1];
			panelPixels[targetIndex + 2] = owner_.paint_buffer_[sourceIndex + 2];
			panelPixels[targetIndex + 3] = owner_.paint_buffer_[sourceIndex + 3];
		}
	}

	if (textureData.texture_image == VK_NULL_HANDLE)
	{
		VkDevice device = owner_.vk_context_->getDevice();

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = targetWidth;
		imageInfo.extent.height = targetHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(device, &imageInfo, nullptr, &textureData.texture_image) != VK_SUCCESS)
		{
			return false;
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, textureData.texture_image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = owner_.findMemoryType(memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &textureData.texture_memory) != VK_SUCCESS)
		{
			return false;
		}

		vkBindImageMemory(device, textureData.texture_image, textureData.texture_memory, 0);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = textureData.texture_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &viewInfo, nullptr, &textureData.texture_view) != VK_SUCCESS)
		{
			return false;
		}

		textureData.texture_layout_initialized = false;
	}

	VkDevice device = owner_.vk_context_->getDevice();

	VkImageSubresource subresource{};
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(device, textureData.texture_image, &subresource, &layout);

	void* data;
	vkMapMemory(device, textureData.texture_memory, 0, VK_WHOLE_SIZE, 0, &data);

	if (layout.rowPitch == targetWidth * 4)
	{
		memcpy(data, panelPixels.data(), targetWidth * targetHeight * 4);
	}
	else
	{
		uint8_t* dataBytes = reinterpret_cast<uint8_t*>(data);
		const uint8_t* srcBytes = panelPixels.data();
		for (int y = 0; y < targetHeight; y++)
		{
			memcpy(dataBytes + (y * layout.rowPitch), srcBytes + (y * targetWidth * 4), targetWidth * 4);
		}
	}

	vkUnmapMemory(device, textureData.texture_memory);

	if (!textureData.texture_layout_initialized)
	{
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = 0;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS)
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = commandPool;
			allocInfo.commandBufferCount = 1;

			if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) == VK_SUCCESS)
			{
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

				vkBeginCommandBuffer(commandBuffer, &beginInfo);

				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = textureData.texture_image;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				vkCmdPipelineBarrier(commandBuffer,
					VK_PIPELINE_STAGE_HOST_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &barrier);

				vkEndCommandBuffer(commandBuffer);

				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;

				vkQueueSubmit(owner_.vk_context_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
				vkQueueWaitIdle(owner_.vk_context_->getGraphicsQueue());

				vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
				textureData.texture_layout_initialized = true;
			}

			vkDestroyCommandPool(device, commandPool, nullptr);
		}
	}

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

	owner_.forceRepaint();

	CefRefPtr<CefBrowserHost> host = browser->GetHost();
	if (host)
	{
		host->WasResized();
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

	owner_.forceRepaint();

	CefRefPtr<CefBrowserHost> host = owner_.browser_->GetHost();
	if (host)
	{
		host->WasResized();
	}
}
