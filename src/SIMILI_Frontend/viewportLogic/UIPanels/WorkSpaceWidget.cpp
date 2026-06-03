#include "WorkspaceWidget.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>

namespace SIMILI {
	namespace Frontend {

	// ========= WidgetRenderHandler =========

	WidgetRenderHandler::WidgetRenderHandler(WorkspaceWidget* owner)
		: owner_(owner)
	{
	}

	void WidgetRenderHandler::GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect& rect)
	{
		if (owner_)
			rect = CefRect(0, 0, owner_->getWidgetWidth(), owner_->getWidgetHeight());
		else
			rect = CefRect(0, 0, 200, 80);
	}

	void WidgetRenderHandler::OnPaint(CefRefPtr<CefBrowser> /*browser*/, PaintElementType type,
		const RectList& /*dirtyRects*/, const void* buffer, int width, int height)
	{
		if (owner_ && type == PET_VIEW)
			owner_->onPaint(buffer, width, height);
	}

	// ========= WidgetCefClient =========

	WidgetCefClient::WidgetCefClient(WorkspaceWidget* owner)
		: owner_(owner)
		, render_handler_(new WidgetRenderHandler(owner))
	{
	}

	CefRefPtr<CefRenderHandler> WidgetCefClient::GetRenderHandler()
	{
		return render_handler_;
	}

	// ========= WorkspaceWidget =========

	WorkspaceWidget::WorkspaceWidget()
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
		, widget_width_(200)
		, widget_height_(80)
		, vulkan_initialized_(false)
		, has_texture_(false)
		, last_ws_x_(-1)
		, last_ws_y_(-1)
		, geometry_dirty_(true)
	{
	}

	WorkspaceWidget::~WorkspaceWidget()
	{
		shutdown();
	}

	bool WorkspaceWidget::initializeVulkan(VKContext* vkContext, VulkanPipeline* pipelines,
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
			std::cerr << "[WorkspaceWidget] createTexture failed" << std::endl;
			return false;
		}

		if (!createVertexBuffer())
		{
			std::cerr << "[WorkspaceWidget] createVertexBuffer failed" << std::endl;
			return false;
		}

		if (!createDescriptorSet())
		{
			std::cerr << "[WorkspaceWidget] createDescriptorSet failed" << std::endl;
			return false;
		}

		if (!createPipeline())
		{
			std::cerr << "[WorkspaceWidget] createPipeline failed" << std::endl;
			return false;
		}

		vulkan_initialized_ = true;
		std::cout << "[WorkspaceWidget] Vulkan resources initialized" << std::endl;
		return true;
	}

	void WorkspaceWidget::loadURL(const std::string& url)
	{
		cef_client_ = new WidgetCefClient(this);

		CefWindowInfo window_info;
		window_info.SetAsWindowless(0);

		CefBrowserSettings browser_settings;
		browser_settings.windowless_frame_rate = 30;
		browser_settings.javascript = STATE_ENABLED;
		browser_settings.background_color = 0; // transparent background

		cef_browser_ = CefBrowserHost::CreateBrowserSync(
			window_info, cef_client_, url, browser_settings, nullptr, nullptr);

		if (!cef_browser_)
			std::cerr << "[WorkspaceWidget] Failed to create browser for: " << url << std::endl;
		else
			std::cout << "[WorkspaceWidget] Browser created for: " << url << std::endl;
	}

	void WorkspaceWidget::sendKeyEvent(const std::string& key)
	{
		if (!cef_browser_)
			return;

		CefRefPtr<CefFrame> frame = cef_browser_->GetMainFrame();
		if (!frame)
			return;

		// Dispatch a synthetic keydown event via JavaScript so the HTML handler receives it
		// regardless of the OS keyboard layout (French: & é " ' map to physical 1 2 3 4)
		std::string js =
			"(function() {"
			"  var evt = new KeyboardEvent('keydown', {"
			"    key: '" + key + "',"
			"    code: 'Digit" + key + "',"
			"    keyCode: " + std::to_string(key[0]) + ","
			"    which: "  + std::to_string(key[0]) + ","
			"    bubbles: true,"
			"    cancelable: true"
			"  });"
			"  document.dispatchEvent(evt);"
			"})();";

		frame->ExecuteJavaScript(js, frame->GetURL(), 0);
	}

	void WorkspaceWidget::shutdown()
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

	void WorkspaceWidget::onPaint(const void* buffer, int width, int height)
	{
		std::lock_guard<std::mutex> lock(paint_mutex_);
		const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
		paint_buffer_.resize(size);
		if (buffer && size > 0)
			std::memcpy(paint_buffer_.data(), buffer, size);
		paint_width_ = width;
		paint_height_ = height;
	}

	void WorkspaceWidget::filterColor(std::vector<unsigned char>& pixelData, int width, int height,
		unsigned char r, unsigned char g, unsigned char b, unsigned char threshold)
	{
		const int pixelCount = width * height;
		for (int i = 0; i < pixelCount; ++i)
		{
			unsigned char* pixel = pixelData.data() + i * 4;
			// CEF paints BGRA: pixel[0]=B, pixel[1]=G, pixel[2]=R, pixel[3]=A
			if (pixel[2] <= static_cast<unsigned char>(r + threshold) &&
				pixel[1] <= static_cast<unsigned char>(g + threshold) &&
				pixel[0] <= static_cast<unsigned char>(b + threshold))
			{
				pixel[3] = 0;
			}
		}
	}

	void WorkspaceWidget::uploadPaintBuffer()
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

		filterColor(localBuffer, w, h, 0, 0, 0, 30);

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

	void WorkspaceWidget::draw(VkCommandBuffer commandBuffer, int wsX, int wsY,
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

	bool WorkspaceWidget::createTexture()
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

	bool WorkspaceWidget::createVertexBuffer()
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

	bool WorkspaceWidget::createDescriptorSet()
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

	bool WorkspaceWidget::createPipeline()
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
			std::cerr << "[WorkspaceWidget] Vertex shader compile error: " << GLSLCompiler::getLastError() << std::endl;
			return false;
		}
		std::vector<uint32_t> fragSPIRV = GLSLCompiler::compileGLSL(fragGLSL, GLSLCompiler::ShaderType::Fragment);
		if (fragSPIRV.empty())
		{
			std::cerr << "[WorkspaceWidget] Fragment shader compile error: " << GLSLCompiler::getLastError() << std::endl;
			return false;
		}

		VkVertexInputAttributeDescription attr0{};
		attr0.binding = 0; attr0.location = 0;
		attr0.format = VK_FORMAT_R32G32_SFLOAT; attr0.offset = 0;

		VkVertexInputAttributeDescription attr1{};
		attr1.binding = 0; attr1.location = 1;
		attr1.format = VK_FORMAT_R32G32_SFLOAT; attr1.offset = 2 * sizeof(float);

		VulkanPipeline::PipelineConfig config;
		config.name = "workspace_widget_mode_ui";
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
			std::cerr << "[WorkspaceWidget] Failed to create pipeline" << std::endl;
			return false;
		}

		std::cout << "[WorkspaceWidget] Pipeline created (handle=" << shared_pipeline_->pipeline << ")" << std::endl;
		return true;
	}

	void WorkspaceWidget::updateGeometry(int wsX, int wsY, int drawableWidth, int drawableHeight)
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

	void WorkspaceWidget::cleanupVulkanResources()
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

	uint32_t WorkspaceWidget::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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