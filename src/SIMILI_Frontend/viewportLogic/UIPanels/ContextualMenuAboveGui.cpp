#include "ContextualMenuAboveGui.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace SIMILI {
	namespace Frontend {

	// ========= ContextualMenuRenderHandler =========

	ContextualMenuRenderHandler::ContextualMenuRenderHandler(ContextualMenuAboveGUI* owner)
		: owner_(owner)
	{
	}

	void ContextualMenuRenderHandler::GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect& rect)
	{
		if (owner_)
			rect = CefRect(0, 0, owner_->getWidgetWidth(), owner_->getWidgetHeight());
		else
			rect = CefRect(0, 0, 400, 300); // sensible default
	}

	void ContextualMenuRenderHandler::OnPaint(CefRefPtr<CefBrowser> /*browser*/, PaintElementType type,
		const RectList& /*dirtyRects*/, const void* buffer, int width, int height)
	{
		if (owner_ && type == PET_VIEW)
			owner_->onPaint(buffer, width, height);
	}

	// ========= ContextualMenuCefClient =========

	ContextualMenuCefClient::ContextualMenuCefClient(ContextualMenuAboveGUI* owner)
		: owner_(owner)
		, render_handler_(new ContextualMenuRenderHandler(owner))
	{
	}

	CefRefPtr<CefRenderHandler> ContextualMenuCefClient::GetRenderHandler()
	{
		return render_handler_;
	}

	// ========= ContextualMenuAboveGUI =========

	ContextualMenuAboveGUI::ContextualMenuAboveGUI()
		: vk_context_(nullptr)
		, vulkan_pipelines_(nullptr)
		, vk_render_pass_(VK_NULL_HANDLE)
		, vk_texture_image_(VK_NULL_HANDLE)
		, vk_texture_memory_(VK_NULL_HANDLE)
		, vk_texture_view_(VK_NULL_HANDLE)
		, vk_sampler_(VK_NULL_HANDLE)
		, vk_descriptor_pool_(VK_NULL_HANDLE)
		, vk_descriptor_set_layout_(VK_NULL_HANDLE)
		, vk_descriptor_set_(VK_NULL_HANDLE)
		, vk_vertex_buffer_(VK_NULL_HANDLE)
		, vk_vertex_buffer_memory_(VK_NULL_HANDLE)
		, paint_width_(0)
		, paint_height_(0)
		, texture_uploaded_width_(0)
		, texture_uploaded_height_(0)
		, bound_texture_view_(VK_NULL_HANDLE)
		, bound_sampler_(VK_NULL_HANDLE)
		, widget_width_(400)
		, widget_height_(300)
		, menu_x_(0)
		, menu_y_(0)
		, vulkan_initialized_(false)
		, has_texture_(false)
		, last_ws_x_(-1)
		, last_ws_y_(-1)
		, geometry_dirty_(true)
		, alpha_percent_(100.0f)
		, prev_mouse_inside_(false)
		, prev_left_button_down_(false)
		, prev_right_button_down_(false)
		, prev_cef_mouse_x_(0)
		, prev_cef_mouse_y_(0)
	{
	}

	ContextualMenuAboveGUI::~ContextualMenuAboveGUI()
	{
		shutdown();
	}

	bool ContextualMenuAboveGUI::initializeVulkan(VKContext* vkContext, VulkanPipeline* pipelines,
		VkRenderPass renderPass, int widgetWidth, int widgetHeight)
	{
		if (vulkan_initialized_)
			return true;

		vk_context_ = vkContext;
		vulkan_pipelines_ = pipelines;
		vk_render_pass_ = renderPass;
		widget_width_ = widgetWidth;
		widget_height_ = widgetHeight;

		if (!createTexture())
		{
			std::cerr << "[ContextualMenuAboveGUI] createTexture failed" << std::endl;
			return false;
		}

		if (!createVertexBuffer())
		{
			std::cerr << "[ContextualMenuAboveGUI] createVertexBuffer failed" << std::endl;
			return false;
		}

		if (!createDescriptorSet())
		{
			std::cerr << "[ContextualMenuAboveGUI] createDescriptorSet failed" << std::endl;
			return false;
		}

		if (!createPipeline())
		{
			std::cerr << "[ContextualMenuAboveGUI] createPipeline failed" << std::endl;
			return false;
		}

		vulkan_initialized_ = true;
		std::cout << "[ContextualMenuAboveGUI] Vulkan resources initialized" << std::endl;
		return true;
	}

	void ContextualMenuAboveGUI::loadURL(const std::string& url)
	{
		cef_client_ = new ContextualMenuCefClient(this);

		CefWindowInfo window_info;
		window_info.SetAsWindowless(0);

		CefBrowserSettings browser_settings;
		browser_settings.windowless_frame_rate = 30;
		browser_settings.javascript = STATE_ENABLED;
		browser_settings.background_color = 0; // transparent background

		cef_browser_ = CefBrowserHost::CreateBrowserSync(
			window_info, cef_client_, url, browser_settings, nullptr, nullptr);

		if (!cef_browser_)
			std::cerr << "[ContextualMenuAboveGUI] Failed to create browser for: " << url << std::endl;
		else
			std::cout << "[ContextualMenuAboveGUI] Browser created for: " << url << std::endl;
	}

	void ContextualMenuAboveGUI::shutdown()
	{
		if (cef_browser_)
		{
			cef_browser_->GetHost()->CloseBrowser(true);
			cef_browser_ = nullptr;
		}
		cef_client_ = nullptr;
		cleanupVulkanResources();
		vulkan_initialized_ = false;
	}

	void ContextualMenuAboveGUI::onPaint(const void* buffer, int width, int height)
	{
		std::lock_guard<std::mutex> lock(paint_mutex_);
		const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
		paint_buffer_.resize(size);
		if (buffer && size > 0)
			std::memcpy(paint_buffer_.data(), buffer, size);
		paint_width_ = width;
		paint_height_ = height;
	}

	void ContextualMenuAboveGUI::makeFullyTransparent(std::vector<unsigned char>& pixelData, int width, int height)
	{
		float alphaFactor = alpha_percent_ / 100.0f;
		if (alphaFactor < 0.0f) alphaFactor = 0.0f;
		if (alphaFactor > 1.0f) alphaFactor = 1.0f;

		const int pixelCount = width * height;
		for (int i = 0; i < pixelCount; ++i)
		{
			unsigned char* pixel = pixelData.data() + i * 4;

			// Background check: if fully black/transparent, make it completely transparent
			if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0)
			{
				pixel[3] = 0;
				continue;
			}

			// Apply general transparency scale
			float currentAlpha = pixel[3];
			pixel[3] = static_cast<unsigned char>(currentAlpha * alphaFactor);
		}
	}

	bool ContextualMenuAboveGUI::handleMouseEvent(int mouseX, int mouseY, bool isLeftButtonDown, bool isRightButtonDown)
	{
		if (!cef_browser_) return false;

		// mouseX and mouseY are in logical window coordinates.
		// Since menu_x_ and menu_y_ are also in logical coordinates:
		const bool isInside = (mouseX >= menu_x_ && mouseX < menu_x_ + widget_width_ &&
		                       mouseY >= menu_y_ && mouseY < menu_y_ + widget_height_);

		if (!isInside)
		{
			if (prev_mouse_inside_)
			{
				CefMouseEvent leaveEvent;
				leaveEvent.x = prev_cef_mouse_x_;
				leaveEvent.y = prev_cef_mouse_y_;
				leaveEvent.modifiers = 0;
				cef_browser_->GetHost()->SendMouseMoveEvent(leaveEvent, true);
				prev_mouse_inside_ = false;
			}
			prev_left_button_down_ = false;
			prev_right_button_down_ = false;
			return false;
		}

		const int localX = mouseX - menu_x_;
		const int localY = mouseY - menu_y_;

		// For the contextual menu, the CEF browser is exactly size widget_width_ by widget_height_.
		// So localX and localY are already the correct coordinates for the CEF mouse event!
		const int cefX = localX;
		const int cefY = localY;

		uint32_t modifiers = 0;
		if (isLeftButtonDown)  modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
		if (isRightButtonDown) modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;

		CefMouseEvent mouseEvent;
		mouseEvent.x = cefX;
		mouseEvent.y = cefY;
		mouseEvent.modifiers = modifiers;

		const bool positionChanged = (!prev_mouse_inside_ || cefX != prev_cef_mouse_x_ || cefY != prev_cef_mouse_y_);
		if (positionChanged)
		{
			cef_browser_->GetHost()->SendMouseMoveEvent(mouseEvent, false);
		}

		if (isLeftButtonDown && !prev_left_button_down_)
		{
			cef_browser_->GetHost()->SendMouseClickEvent(mouseEvent, MBT_LEFT, false, 1);
		}
		else if (!isLeftButtonDown && prev_left_button_down_)
		{
			cef_browser_->GetHost()->SendMouseClickEvent(mouseEvent, MBT_LEFT, true, 1);
		}

		if (isRightButtonDown && !prev_right_button_down_)
		{
			cef_browser_->GetHost()->SendMouseClickEvent(mouseEvent, MBT_RIGHT, false, 1);
		}
		else if (!isRightButtonDown && prev_right_button_down_)
		{
			cef_browser_->GetHost()->SendMouseClickEvent(mouseEvent, MBT_RIGHT, true, 1);
		}

		prev_mouse_inside_ = true;
		prev_left_button_down_ = isLeftButtonDown;
		prev_right_button_down_ = isRightButtonDown;
		prev_cef_mouse_x_ = cefX;
		prev_cef_mouse_y_ = cefY;

		return true;
	}

	void ContextualMenuAboveGUI::uploadPaintBuffer()
	{
		if (!vulkan_initialized_ || !vk_context_)
			return;

		std::vector<unsigned char> localBuffer;
		int w = 0, h = 0;
		{
			std::lock_guard<std::mutex> lock(paint_mutex_);
			if (paint_buffer_.empty() || paint_width_ <= 0 || paint_height_ <= 0)
				return;
			localBuffer = paint_buffer_;
			w = paint_width_;
			h = paint_height_;
		}

		makeFullyTransparent(localBuffer, w, h);

		VkDevice device = vk_context_->getDevice();

		// Recreate texture if size changed
		if (vk_texture_image_ != VK_NULL_HANDLE &&
			(w != texture_uploaded_width_ || h != texture_uploaded_height_))
		{
			vkDeviceWaitIdle(device);
			if (vk_texture_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device, vk_texture_view_, nullptr); vk_texture_view_ = VK_NULL_HANDLE; }
			if (vk_sampler_ != VK_NULL_HANDLE)      { vkDestroySampler(device, vk_sampler_, nullptr); vk_sampler_ = VK_NULL_HANDLE; }
			if (vk_texture_image_ != VK_NULL_HANDLE){ vkDestroyImage(device, vk_texture_image_, nullptr); vk_texture_image_ = VK_NULL_HANDLE; }
			if (vk_texture_memory_ != VK_NULL_HANDLE){ vkFreeMemory(device, vk_texture_memory_, nullptr); vk_texture_memory_ = VK_NULL_HANDLE; }
			bound_texture_view_ = VK_NULL_HANDLE;
			bound_sampler_ = VK_NULL_HANDLE;
			has_texture_ = false;
			texture_uploaded_width_ = 0;
			texture_uploaded_height_ = 0;
		}

		// Create DEVICE_LOCAL texture for upload
		if (vk_texture_image_ == VK_NULL_HANDLE)
		{
			VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
			imgInfo.imageType = VK_IMAGE_TYPE_2D;
			imgInfo.extent = { (uint32_t)w, (uint32_t)h, 1 };
			imgInfo.mipLevels = 1;
			imgInfo.arrayLayers = 1;
			imgInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
			imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			if (vkCreateImage(device, &imgInfo, nullptr, &vk_texture_image_) != VK_SUCCESS) return;

			VkMemoryRequirements memReq;
			vkGetImageMemoryRequirements(device, vk_texture_image_, &memReq);
			VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
			allocInfo.allocationSize = memReq.size;
			allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			if (vkAllocateMemory(device, &allocInfo, nullptr, &vk_texture_memory_) != VK_SUCCESS)
			{
				vkDestroyImage(device, vk_texture_image_, nullptr); vk_texture_image_ = VK_NULL_HANDLE;
				return;
			}
			vkBindImageMemory(device, vk_texture_image_, vk_texture_memory_, 0);

			VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = vk_texture_image_;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			if (vkCreateImageView(device, &viewInfo, nullptr, &vk_texture_view_) != VK_SUCCESS) return;

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.anisotropyEnable = VK_FALSE;
			samplerInfo.maxAnisotropy = 1.0f;
			samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
			samplerInfo.unnormalizedCoordinates = VK_FALSE;
			samplerInfo.compareEnable = VK_FALSE;
			samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			if (vkCreateSampler(device, &samplerInfo, nullptr, &vk_sampler_) != VK_SUCCESS) return;
		}

		// Staging buffer upload
		const VkDeviceSize bufSize = static_cast<VkDeviceSize>(w) * h * 4;

		VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		stagingInfo.size = bufSize;
		stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkBuffer staging = VK_NULL_HANDLE;
		if (vkCreateBuffer(device, &stagingInfo, nullptr, &staging) != VK_SUCCESS) return;

		VkMemoryRequirements stagingReq;
		vkGetBufferMemoryRequirements(device, staging, &stagingReq);
		VkMemoryAllocateInfo stagingAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		stagingAlloc.allocationSize = stagingReq.size;
		stagingAlloc.memoryTypeIndex = findMemoryType(stagingReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VkDeviceMemory stagingMem = VK_NULL_HANDLE;
		if (vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingMem) != VK_SUCCESS)
		{
			vkDestroyBuffer(device, staging, nullptr);
			return;
		}
		vkBindBufferMemory(device, staging, stagingMem, 0);

		void* mapped = nullptr;
		vkMapMemory(device, stagingMem, 0, bufSize, 0, &mapped);
		std::memcpy(mapped, localBuffer.data(), static_cast<size_t>(bufSize));
		vkUnmapMemory(device, stagingMem);

		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.queueFamilyIndex = vk_context_->getGraphicsQueueFamily();
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkCommandPool pool = VK_NULL_HANDLE;
		vkCreateCommandPool(device, &poolInfo, nullptr, &pool);

		VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmdAlloc.commandPool = pool;
		cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdAlloc.commandBufferCount = 1;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);

		VkImageMemoryBarrier toTransfer{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		toTransfer.oldLayout = (texture_uploaded_width_ == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
		toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toTransfer.srcQueueFamilyIndex = toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toTransfer.image = vk_texture_image_;
		toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		toTransfer.srcAccessMask = (texture_uploaded_width_ == 0) ? 0 : VK_ACCESS_SHADER_READ_BIT;
		toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &toTransfer);

		VkBufferImageCopy region{};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
		vkCmdCopyBufferToImage(cmd, staging, vk_texture_image_,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier toGeneral{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		toGeneral.srcQueueFamilyIndex = toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		toGeneral.image = vk_texture_image_;
		toGeneral.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &toGeneral);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &cmd;
		vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
		vkQueueWaitIdle(vk_context_->getGraphicsQueue());

		vkDestroyCommandPool(device, pool, nullptr);
		vkFreeMemory(device, stagingMem, nullptr);
		vkDestroyBuffer(device, staging, nullptr);

		texture_uploaded_width_ = w;
		texture_uploaded_height_ = h;
		has_texture_ = true;
		geometry_dirty_ = true;

		// Update descriptor set if texture view changed
		if (vk_descriptor_set_ != VK_NULL_HANDLE &&
			(bound_texture_view_ != vk_texture_view_ || bound_sampler_ != vk_sampler_))
		{
			vkDeviceWaitIdle(device);
			VkDescriptorImageInfo imgInfo{};
			imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			imgInfo.imageView = vk_texture_view_;
			imgInfo.sampler = vk_sampler_;

			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.dstSet = vk_descriptor_set_;
			write.dstBinding = 0;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.descriptorCount = 1;
			write.pImageInfo = &imgInfo;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

			bound_texture_view_ = vk_texture_view_;
			bound_sampler_ = vk_sampler_;
		}
	}

	void ContextualMenuAboveGUI::draw(VkCommandBuffer commandBuffer, int wsX, int wsY,
		int drawableWidth, int drawableHeight)
	{
		if (!vulkan_initialized_ || !has_texture_)
			return;
		if (commandBuffer == VK_NULL_HANDLE)
			return;
		if (!shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
			return;
		if (vk_descriptor_set_ == VK_NULL_HANDLE || vk_vertex_buffer_ == VK_NULL_HANDLE)
			return;

		if (geometry_dirty_ || wsX != last_ws_x_ || wsY != last_ws_y_)
		{
			updateGeometry(wsX, wsY, drawableWidth, drawableHeight);
			last_ws_x_ = wsX;
			last_ws_y_ = wsY;
			geometry_dirty_ = false;
		}

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->pipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			shared_pipeline_->layout, 0, 1, &vk_descriptor_set_, 0, nullptr);

		VkBuffer vertexBuffers[] = { vk_vertex_buffer_ };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}

	// ===== Private =====

	bool ContextualMenuAboveGUI::createTexture()
	{
		if (!vk_context_) return false;
		VkDevice device = vk_context_->getDevice();

		// 1x1 placeholder (LINEAR + HOST_VISIBLE, no staging needed)
		unsigned char pixel[4] = { 0, 0, 0, 0 };

		VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { 1, 1, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers  = 1;
		imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode  = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(device, &imageInfo, nullptr, &vk_texture_image_) != VK_SUCCESS) return false;

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(device, vk_texture_image_, &memReq);
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize  = memReq.size;
		allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (vkAllocateMemory(device, &allocInfo, nullptr, &vk_texture_memory_) != VK_SUCCESS)
		{
			vkDestroyImage(device, vk_texture_image_, nullptr);
			vk_texture_image_ = VK_NULL_HANDLE;
			return false;
		}
		vkBindImageMemory(device, vk_texture_image_, vk_texture_memory_, 0);

		void* mapped = nullptr;
		vkMapMemory(device, vk_texture_memory_, 0, memReq.size, 0, &mapped);
		std::memcpy(mapped, pixel, 4);
		vkUnmapMemory(device, vk_texture_memory_);

		// Transition UNDEFINED → GENERAL
		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.queueFamilyIndex = vk_context_->getGraphicsQueueFamily();
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		VkCommandPool pool = VK_NULL_HANDLE;
		vkCreateCommandPool(device, &poolInfo, nullptr, &pool);

		VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmdAlloc.commandPool = pool; cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cmdAlloc.commandBufferCount = 1;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &beginInfo);

		VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = vk_texture_image_;
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		barrier.srcAccessMask = 0; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		vkEndCommandBuffer(cmd);
		VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
		vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
		vkQueueWaitIdle(vk_context_->getGraphicsQueue());
		vkDestroyCommandPool(device, pool, nullptr);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk_texture_image_;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		if (vkCreateImageView(device, &viewInfo, nullptr, &vk_texture_view_) != VK_SUCCESS) return false;

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		if (vkCreateSampler(device, &samplerInfo, nullptr, &vk_sampler_) != VK_SUCCESS) return false;

		return true;
	}

	bool ContextualMenuAboveGUI::createVertexBuffer()
	{
		if (!vk_context_) return false;
		VkDevice device = vk_context_->getDevice();

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeof(float) * 24; // 6 vertices × 4 floats
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(device, &bufferInfo, nullptr, &vk_vertex_buffer_) != VK_SUCCESS) return false;

		VkMemoryRequirements memReq;
		vkGetBufferMemoryRequirements(device, vk_vertex_buffer_, &memReq);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (vkAllocateMemory(device, &allocInfo, nullptr, &vk_vertex_buffer_memory_) != VK_SUCCESS)
		{
			vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
			vk_vertex_buffer_ = VK_NULL_HANDLE;
			return false;
		}
		vkBindBufferMemory(device, vk_vertex_buffer_, vk_vertex_buffer_memory_, 0);
		return true;
	}

	bool ContextualMenuAboveGUI::createDescriptorSet()
	{
		if (!vk_context_) return false;
		VkDevice device = vk_context_->getDevice();

		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &vk_descriptor_set_layout_) != VK_SUCCESS)
			return false;

		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = 1;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &vk_descriptor_pool_) != VK_SUCCESS)
		{
			vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
			vk_descriptor_set_layout_ = VK_NULL_HANDLE;
			return false;
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = vk_descriptor_pool_;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &vk_descriptor_set_layout_;
		if (vkAllocateDescriptorSets(device, &allocInfo, &vk_descriptor_set_) != VK_SUCCESS) return false;

		VkDescriptorImageInfo imgInfo{};
		imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imgInfo.imageView = vk_texture_view_;
		imgInfo.sampler = vk_sampler_;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = vk_descriptor_set_;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imgInfo;
		vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

		bound_texture_view_ = vk_texture_view_;
		bound_sampler_ = vk_sampler_;
		return true;
	}

	bool ContextualMenuAboveGUI::createPipeline()
	{
		if (!vk_context_ || !vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
			return false;
		if (vk_render_pass_ == VK_NULL_HANDLE || vk_descriptor_set_layout_ == VK_NULL_HANDLE)
			return false;

		const std::string vertGLSL = R"(
		#version 450
		layout(location = 0) in vec2 aPosition;
		layout(location = 1) in vec2 aTexCoord;
		layout(location = 0) out vec2 vTexCoord;
		void main() {
			gl_Position = vec4(aPosition, 0.0, 1.0);
			vTexCoord = aTexCoord;
		}
		)";

		const std::string fragGLSL = R"(
		#version 450
		layout(location = 0) in vec2 vTexCoord;
		layout(location = 0) out vec4 outColor;
		layout(binding = 0) uniform sampler2D texSampler;
		void main() {
			outColor = texture(texSampler, vTexCoord);
		}
		)";

		std::vector<uint32_t> vertSPIRV = GLSLCompiler::compileGLSL(vertGLSL, GLSLCompiler::ShaderType::Vertex);
		if (vertSPIRV.empty())
		{
			std::cerr << "[ContextualMenuAboveGUI] Vertex shader compile error: " << GLSLCompiler::getLastError() << std::endl;
			return false;
		}
		std::vector<uint32_t> fragSPIRV = GLSLCompiler::compileGLSL(fragGLSL, GLSLCompiler::ShaderType::Fragment);
		if (fragSPIRV.empty())
		{
			std::cerr << "[ContextualMenuAboveGUI] Fragment shader compile error: " << GLSLCompiler::getLastError() << std::endl;
			return false;
		}

		VkVertexInputAttributeDescription attr0{};
		attr0.binding = 0; attr0.location = 0;
		attr0.format = VK_FORMAT_R32G32_SFLOAT; attr0.offset = 0;

		VkVertexInputAttributeDescription attr1{};
		attr1.binding = 0; attr1.location = 1;
		attr1.format = VK_FORMAT_R32G32_SFLOAT; attr1.offset = 2 * sizeof(float);

		VulkanPipeline::PipelineConfig config;
		config.name = "contextual_menu_above_gui";
		config.vertexShaderCode = GLSLCompiler::spirvToBytes(vertSPIRV);
		config.fragmentShaderCode = GLSLCompiler::spirvToBytes(fragSPIRV);
		config.renderPass = vk_render_pass_;
		config.descriptorSetLayout = vk_descriptor_set_layout_;
		config.enableBlending = true;
		config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		config.vertexBindingStride = 4 * sizeof(float);
		config.vertexAttributes = { attr0, attr1 };

		shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);
		if (!shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
		{
			std::cerr << "[ContextualMenuAboveGUI] Failed to create pipeline" << std::endl;
			return false;
		}

		std::cout << "[ContextualMenuAboveGUI] Pipeline created (handle=" << shared_pipeline_->pipeline << ")" << std::endl;
		return true;
	}

	void ContextualMenuAboveGUI::updateGeometry(int wsX, int wsY, int drawableWidth, int drawableHeight)
	{
		if (!vk_context_ || vk_vertex_buffer_ == VK_NULL_HANDLE) return;
		if (drawableWidth <= 0 || drawableHeight <= 0) return;

		const int left = wsX;
		const int top = wsY;
		const int right = wsX + widget_width_;
		const int bottom = wsY + widget_height_;

		const float ndcLeft = (static_cast<float>(left)   / static_cast<float>(drawableWidth))  * 2.0f - 1.0f;
		const float ndcRight = (static_cast<float>(right)  / static_cast<float>(drawableWidth))  * 2.0f - 1.0f;
		const float ndcTop = (static_cast<float>(top)    / static_cast<float>(drawableHeight)) * 2.0f - 1.0f;
		const float ndcBottom = (static_cast<float>(bottom) / static_cast<float>(drawableHeight)) * 2.0f - 1.0f;

		const float vertices[] = {
			ndcLeft, ndcTop, 0.0f, 0.0f,
			ndcRight, ndcBottom, 1.0f, 1.0f,
			ndcLeft, ndcBottom, 0.0f, 1.0f,
			ndcLeft, ndcTop, 0.0f, 0.0f,
			ndcRight, ndcTop, 1.0f, 0.0f,
			ndcRight, ndcBottom, 1.0f, 1.0f,
		};

		void* data = nullptr;
		if (vkMapMemory(vk_context_->getDevice(), vk_vertex_buffer_memory_,
			0, sizeof(vertices), 0, &data) == VK_SUCCESS)
		{
			std::memcpy(data, vertices, sizeof(vertices));
			vkUnmapMemory(vk_context_->getDevice(), vk_vertex_buffer_memory_);
		}
	}

	void ContextualMenuAboveGUI::cleanupVulkanResources()
	{
		if (!vk_context_) return;
		VkDevice device = vk_context_->getDevice();
		vkDeviceWaitIdle(device);

		if (vk_descriptor_pool_ != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(device, vk_descriptor_pool_, nullptr);
			vk_descriptor_pool_ = VK_NULL_HANDLE;
			vk_descriptor_set_ = VK_NULL_HANDLE;
		}
		if (vk_descriptor_set_layout_ != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
			vk_descriptor_set_layout_ = VK_NULL_HANDLE;
		}
		shared_pipeline_.reset();
		if (vk_vertex_buffer_memory_ != VK_NULL_HANDLE)
		{
			vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
			vk_vertex_buffer_memory_ = VK_NULL_HANDLE;
		}
		if (vk_vertex_buffer_ != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
			vk_vertex_buffer_ = VK_NULL_HANDLE;
		}
		if (vk_sampler_ != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, vk_sampler_, nullptr);
			vk_sampler_ = VK_NULL_HANDLE;
		}
		if (vk_texture_view_ != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, vk_texture_view_, nullptr);
			vk_texture_view_ = VK_NULL_HANDLE;
		}
		if (vk_texture_memory_ != VK_NULL_HANDLE)
		{
			vkFreeMemory(device, vk_texture_memory_, nullptr);
			vk_texture_memory_ = VK_NULL_HANDLE;
		}
		if (vk_texture_image_ != VK_NULL_HANDLE)
		{
			vkDestroyImage(device, vk_texture_image_, nullptr);
			vk_texture_image_ = VK_NULL_HANDLE;
		}
		bound_texture_view_ = VK_NULL_HANDLE;
		bound_sampler_ = VK_NULL_HANDLE;
		has_texture_ = false;
	}

	uint32_t ContextualMenuAboveGUI::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1u << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}
		return 0;
	}

	} // namespace Frontend
} // namespace SIMILI
