#define GLM_ENABLE_EXPERIMENTAL

#include "UIPanel.hpp"
#include "../../CEFDrawing/CEF_Drawer.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <cstring>

UIPanel::UIPanel()
	: red_(0.0f)
	, green_(0.0f)
	, blue_(0.0f)
	, x_(0)
	, y_(0)
	, width_(0)
	, height_(0)
	, last_frame_x_(-1)
	, last_frame_y_(-1)
	, last_frame_width_(-1)
	, last_frame_height_(-1)
	, texcoord_left_(0.0f)
	, texcoord_top_(0.0f)
	, texcoord_right_(1.0f)
	, texcoord_bottom_(1.0f)
	, initialized_(false)
	, has_valid_bounds_(false)
	, needs_redraw_(false)
	, first_draw_done_(false)
	, drawing_state_(DrawingState::IsNotReadyToBeDrawn)
	, vk_context_(nullptr)
	, vulkan_pipelines_(nullptr)
	, vk_render_pass_(VK_NULL_HANDLE)
	, vk_descriptor_pool_(VK_NULL_HANDLE)
	, vk_descriptor_set_layout_(VK_NULL_HANDLE)
	, vk_texture_image_(VK_NULL_HANDLE)
	, vk_texture_memory_(VK_NULL_HANDLE)
	, vk_texture_view_(VK_NULL_HANDLE)
	, vk_sampler_(VK_NULL_HANDLE)
	, vk_descriptor_set_(VK_NULL_HANDLE)
	, shared_pipeline_(nullptr)
	, vk_vertex_buffer_(VK_NULL_HANDLE)
	, vk_vertex_buffer_memory_(VK_NULL_HANDLE)
	, external_texture_view_(VK_NULL_HANDLE)
	, external_sampler_(VK_NULL_HANDLE)
	, bound_texture_view_(VK_NULL_HANDLE)
	, bound_sampler_(VK_NULL_HANDLE)
{
}

UIPanel::~UIPanel()
{
	cleanupVulkanResources();
	shutdown();
}

bool UIPanel::initialize(const std::string& panelName)
{
	if (initialized_)
	{
		return true;
	}

	name_ = panelName;

	std::size_t hashValue = std::hash<std::string>{}(name_);
	red_ = 0.25f + (static_cast<float>(hashValue & 0xFFu) / 255.0f) * 0.55f;
	green_ = 0.25f + (static_cast<float>((hashValue >> 8) & 0xFFu) / 255.0f) * 0.55f;
	blue_ = 0.25f + (static_cast<float>((hashValue >> 16) & 0xFFu) / 255.0f) * 0.55f;

	initialized_ = true;
	return true;
}

void UIPanel::shutdown()
{
	cleanupVulkanResources();

	initialized_ = false;
	has_valid_bounds_ = false;
	needs_redraw_ = false;
	drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
	external_texture_view_ = VK_NULL_HANDLE;
	external_sampler_ = VK_NULL_HANDLE;
	bound_texture_view_ = VK_NULL_HANDLE;
	bound_sampler_ = VK_NULL_HANDLE;
	last_frame_x_ = 0;
	last_frame_y_ = 0;
	last_frame_width_ = 0;
	last_frame_height_ = 0;
	texcoord_left_ = 0.0f;
	texcoord_top_ = 0.0f;
	texcoord_right_ = 1.0f;
	texcoord_bottom_ = 1.0f;
}

void UIPanel::updateFromFrameData(const SIMILI::Frontend::IFrameScreenData& frameData, SDL_Window* window, bool skipTextureRebuild)
{
	if (!window)
	{
		has_valid_bounds_ = false;
		return;
	}

	int logicalWindowWidth = 0;
	int logicalWindowHeight = 0;
	int drawableWidth = 0;
	int drawableHeight = 0;
	SDL_GetWindowSize(window, &logicalWindowWidth, &logicalWindowHeight);
	SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);

	if (logicalWindowWidth <= 0 || logicalWindowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0)
	{
		has_valid_bounds_ = false;
		return;
	}

	const int MIN_FRAME_DIMENSION = 10;
	if (frameData.width < MIN_FRAME_DIMENSION || frameData.height < MIN_FRAME_DIMENSION)
	{
		static int skip_count = 0;
		has_valid_bounds_ = false;
		return;
	}

	float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(logicalWindowWidth);
	float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(logicalWindowHeight);

	x_ = static_cast<int>(std::lround(static_cast<float>(frameData.relativeX) * scaleX));
	y_ = static_cast<int>(std::lround(static_cast<float>(frameData.relativeY) * scaleY));
	width_ = static_cast<int>(std::lround(static_cast<float>(frameData.width) * scaleX));
	height_ = static_cast<int>(std::lround(static_cast<float>(frameData.height) * scaleY));


	if (x_ < 0)
	{
		width_ += x_;
		x_ = 0;
		if (width_ < 1)
		{
			width_ = 1;
		}
	}

	if (y_ < 0)
	{
		height_ += y_;
		y_ = 0;
		if (height_ < 1)
		{
			height_ = 1;
		}
	}

	if (x_ + width_ > drawableWidth)
	{
		width_ = (std::max)(1, drawableWidth - x_);
	}

	if (y_ + height_ > drawableHeight)
	{
		height_ = (std::max)(1, drawableHeight - y_);
	}

	has_valid_bounds_ = width_ > 0 && height_ > 0;

	// Update display frame with SDL drawable coordinates (with DPI scaling)
	CEF_Drawer::UIPanelFrameData displayFrame;
	displayFrame.x = x_;
	displayFrame.y = y_;
	displayFrame.width = width_;
	displayFrame.height = height_;
	CEF_Drawer::updateActiveUIPanelDisplayFrame(name_, displayFrame);

	// Update source frame with CEF logical coordinates (without DPI scaling)
	CEF_Drawer::UIPanelFrameData sourceFrame;
	sourceFrame.x = frameData.relativeX;
	sourceFrame.y = frameData.relativeY;
	sourceFrame.width = frameData.width;
	sourceFrame.height = frameData.height;
	CEF_Drawer::updateActiveUIPanelSourceFrame(name_, sourceFrame);

	const bool frameGeometryChanged =
		frameData.relativeX != last_frame_x_ || frameData.relativeY != last_frame_y_ ||
		frameData.width != last_frame_width_ || frameData.height != last_frame_height_;

	if (frameGeometryChanged)
	{
		last_frame_x_ = frameData.relativeX;
		last_frame_y_ = frameData.relativeY;
		last_frame_width_ = frameData.width;
		last_frame_height_ = frameData.height;
		needs_redraw_ = true;
	}

	const bool hasNoTexture = (external_texture_view_ == VK_NULL_HANDLE);

	if (!has_valid_bounds_)
	{
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}

	if (!skipTextureRebuild)
	{
		const bool isTextureDirty = CEF_Drawer::isActiveUIPanelTextureDirty(name_);
		if (frameGeometryChanged || isTextureDirty || hasNoTexture)
		{
			needs_redraw_ = true;
			updateTextureRegion();
		}
	}

	if (has_valid_bounds_ && shared_pipeline_ && vk_vertex_buffer_ != VK_NULL_HANDLE)
	{
		if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
		{
			drawing_state_ = DrawingState::IsReadyToBeDrawn;
			needs_redraw_ = true;
		}
	}
}

void UIPanel::draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}
	
	if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
	{
		static int not_ready_log_count = 0;
		if (not_ready_log_count < 5 || not_ready_log_count % 60 == 0)
		{
			std::cout << "[UIPanel::draw] " << name_ << " - IsNotReadyToBeDrawn (count=" << not_ready_log_count << ")" << std::endl;
		}
		not_ready_log_count++;
		return;
	}
	
	if (!initialized_ || !has_valid_bounds_ || drawableWidth <= 0 || drawableHeight <= 0)
	{
		static int invalid_state_log_count = 0;
		if (invalid_state_log_count < 5 || invalid_state_log_count % 60 == 0)
		{
			std::cout << "[UIPanel::draw] " << name_ << " - Invalid state: initialized=" << initialized_ 
			          << " has_valid_bounds=" << has_valid_bounds_ 
			          << " drawableSize=" << drawableWidth << "x" << drawableHeight 
			          << " (count=" << invalid_state_log_count << ")" << std::endl;
		}
		invalid_state_log_count++;
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}

	if (!vk_context_ || !shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		static int missing_vulkan_log_count = 0;
		if (missing_vulkan_log_count < 5 || missing_vulkan_log_count % 60 == 0)
		{
			std::cout << "[UIPanel::draw] " << name_ << " - Missing Vulkan resources: vk_context=" << (vk_context_ != nullptr)
			          << " pipeline=" << (shared_pipeline_ ? shared_pipeline_->pipeline : VK_NULL_HANDLE)
			          << " (count=" << missing_vulkan_log_count << ")" << std::endl;
		}
		missing_vulkan_log_count++;
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}

	VkImageView textureToUse = external_texture_view_;
	VkSampler samplerToUse = external_sampler_;
	
	if (textureToUse == VK_NULL_HANDLE || samplerToUse == VK_NULL_HANDLE)
	{
		if (vk_texture_view_ != VK_NULL_HANDLE && vk_sampler_ != VK_NULL_HANDLE)
		{
			static int using_fallback_log_count = 0;
			if (using_fallback_log_count < 5 || using_fallback_log_count % 60 == 0)
			{
				std::cout << "[UIPanel::draw] " << name_ << " - Using fallback internal texture (count=" << using_fallback_log_count << ")" << std::endl;
			}
			using_fallback_log_count++;
			textureToUse = vk_texture_view_;
			samplerToUse = vk_sampler_;
		}
		else
		{
			static int missing_texture_log_count = 0;
			if (missing_texture_log_count < 5 || missing_texture_log_count % 60 == 0)
			{
				std::cout << "[UIPanel::draw] " << name_ << " - Missing all textures (count=" << missing_texture_log_count << ")" << std::endl;
			}
			missing_texture_log_count++;
			drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
			return;
		}
	}

	VkImageView textureForBinding = (textureToUse != VK_NULL_HANDLE) ? textureToUse : vk_texture_view_;
	VkSampler samplerForBinding = (samplerToUse != VK_NULL_HANDLE) ? samplerToUse : vk_sampler_;
	
	if (textureForBinding != VK_NULL_HANDLE && samplerForBinding != VK_NULL_HANDLE)
	{
		if (bound_texture_view_ != textureForBinding || bound_sampler_ != samplerForBinding)
		{
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.imageView = textureForBinding;
			imageInfo.sampler = samplerForBinding;
			
			VkWriteDescriptorSet descriptorWrite{};
			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstSet = vk_descriptor_set_;
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pImageInfo = &imageInfo;
			
			vkUpdateDescriptorSets(vk_context_->getDevice(), 1, &descriptorWrite, 0, nullptr);
			
			bound_texture_view_ = textureForBinding;
			bound_sampler_ = samplerForBinding;
		}
	}
	else
	{
		static int descriptor_fail_log_count = 0;
		if (descriptor_fail_log_count < 5 || descriptor_fail_log_count % 60 == 0)
		{
			std::cout << "[UIPanel::draw] " << name_ << " - No valid texture for binding (count=" << descriptor_fail_log_count << ")" << std::endl;
		}
		descriptor_fail_log_count++;
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}
	
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(drawableWidth);
	viewport.height = static_cast<float>(drawableHeight);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = {static_cast<uint32_t>(drawableWidth), static_cast<uint32_t>(drawableHeight)};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->pipeline);

	if (vk_descriptor_set_ != VK_NULL_HANDLE)
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->layout, 0, 1, &vk_descriptor_set_, 0, nullptr);
	}

	if (vk_vertex_buffer_ != VK_NULL_HANDLE)
	{
		if (needs_redraw_ || drawing_state_ == DrawingState::IsReadyToBeDrawn)
		{
			updateGeometry(drawableWidth, drawableHeight);
			needs_redraw_ = false;
		}
		
		VkBuffer vertexBuffers[] = {vk_vertex_buffer_};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		
		if (drawing_state_ == DrawingState::IsReadyToBeDrawn)
		{
			drawing_state_ = DrawingState::HasBeenDrawn;
		}
		
		needs_redraw_ = false;
	}
}

void UIPanel::updateTextureRegion()
{
	VkImageView oldTextureView = external_texture_view_;
	VkSampler oldSampler = external_sampler_;
	float oldTexcoordLeft = texcoord_left_;
	float oldTexcoordTop = texcoord_top_;
	float oldTexcoordRight = texcoord_right_;
	float oldTexcoordBottom = texcoord_bottom_;
	
	VkImageView textureView = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	int textureWidth = 0;
	int textureHeight = 0;
	CEF_Drawer::UIPanelFrameData panelFrame = {};

	if (!CEF_Drawer::getActiveUIPanelTextureRegion(name_, textureView, sampler, textureWidth, textureHeight, panelFrame))
	{

		
		if (oldTextureView != VK_NULL_HANDLE && oldSampler != VK_NULL_HANDLE)
		{
			
			if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
			{
				drawing_state_ = DrawingState::HasBeenDrawn;
			}
			needs_redraw_ = true;
			
			return;
		}
		
		external_texture_view_ = VK_NULL_HANDLE;
		external_sampler_ = VK_NULL_HANDLE;
		texcoord_left_ = 0.0f;
		texcoord_top_ = 0.0f;
		texcoord_right_ = 1.0f;
		texcoord_bottom_ = 1.0f;
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}

	if (textureWidth <= 0 || textureHeight <= 0)
	{
		if (oldTextureView != VK_NULL_HANDLE && oldSampler != VK_NULL_HANDLE)
		{
			if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
			{
				drawing_state_ = DrawingState::HasBeenDrawn;
			}
			needs_redraw_ = true;
			return;
		}
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}
	
	if (textureView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
	{
		if (oldTextureView != VK_NULL_HANDLE && oldSampler != VK_NULL_HANDLE)
		{
			if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
			{
				drawing_state_ = DrawingState::HasBeenDrawn;
			}
			needs_redraw_ = true;
			return;
		}
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}

	float left = static_cast<float>(panelFrame.x) / static_cast<float>(textureWidth);
	float top = static_cast<float>(panelFrame.y) / static_cast<float>(textureHeight);
	float right = static_cast<float>(panelFrame.x + panelFrame.width) / static_cast<float>(textureWidth);
	float bottom = static_cast<float>(panelFrame.y + panelFrame.height) / static_cast<float>(textureHeight);

	left = (std::clamp)(left, 0.0f, 1.0f);
	top = (std::clamp)(top, 0.0f, 1.0f);
	right = (std::clamp)(right, 0.0f, 1.0f);
	bottom = (std::clamp)(bottom, 0.0f, 1.0f);

	if (right <= left || bottom <= top)
	{
		if (oldTextureView != VK_NULL_HANDLE && oldSampler != VK_NULL_HANDLE)
		{
			if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
			{
				drawing_state_ = DrawingState::HasBeenDrawn;
			}
			needs_redraw_ = true;
			return;
		}
		drawing_state_ = DrawingState::IsNotReadyToBeDrawn;
		return;
	}

	external_texture_view_ = textureView;
	external_sampler_ = sampler;
	texcoord_left_ = left;
	texcoord_top_ = top;
	texcoord_right_ = right;
	texcoord_bottom_ = bottom;
		
	if (drawing_state_ == DrawingState::IsNotReadyToBeDrawn)
	{
		drawing_state_ = DrawingState::IsReadyToBeDrawn;
		needs_redraw_ = true;
	}
}

bool UIPanel::updateDescriptorTextureBinding()
{
	if (!vk_context_ || vk_descriptor_set_ == VK_NULL_HANDLE)
	{
		return false;
	}

	VkImageView textureView = external_texture_view_ != VK_NULL_HANDLE ? external_texture_view_ : vk_texture_view_;
	VkSampler sampler = external_sampler_ != VK_NULL_HANDLE ? external_sampler_ : vk_sampler_;

	if (textureView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
	{
		return false;
	}

	if (bound_texture_view_ == textureView && bound_sampler_ == sampler)
	{
		return true;
	}

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = textureView;
	imageInfo.sampler = sampler;

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = vk_descriptor_set_;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(vk_context_->getDevice(), 1, &descriptorWrite, 0, nullptr);

	bound_texture_view_ = textureView;
	bound_sampler_ = sampler;
	
	return true;
}

bool UIPanel::createTexture()
{
	if (!vk_context_)
	{
		return false;
	}

	VkDevice device = vk_context_->getDevice();

	unsigned char pixel[4] = {
		static_cast<unsigned char>(std::lround(red_ * 255.0f)),
		static_cast<unsigned char>(std::lround(green_ * 255.0f)),
		static_cast<unsigned char>(std::lround(blue_ * 255.0f)),
		255
	};

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = 1;
	imageInfo.extent.height = 1;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imageInfo, nullptr, &vk_texture_image_) != VK_SUCCESS)
	{
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, vk_texture_image_, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &vk_texture_memory_) != VK_SUCCESS)
	{
		vkDestroyImage(device, vk_texture_image_, nullptr);
		vk_texture_image_ = VK_NULL_HANDLE;
		return false;
	}

	vkBindImageMemory(device, vk_texture_image_, vk_texture_memory_, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vk_texture_image_;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &viewInfo, nullptr, &vk_texture_view_) != VK_SUCCESS)
	{
		vkFreeMemory(device, vk_texture_memory_, nullptr);
		vkDestroyImage(device, vk_texture_image_, nullptr);
		vk_texture_memory_ = VK_NULL_HANDLE;
		vk_texture_image_ = VK_NULL_HANDLE;
		return false;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &vk_sampler_) != VK_SUCCESS)
	{
		vkDestroyImageView(device, vk_texture_view_, nullptr);
		vkFreeMemory(device, vk_texture_memory_, nullptr);
		vkDestroyImage(device, vk_texture_image_, nullptr);
		vk_texture_view_ = VK_NULL_HANDLE;
		vk_texture_memory_ = VK_NULL_HANDLE;
		vk_texture_image_ = VK_NULL_HANDLE;
		return false;
	}

	return true;
}

bool UIPanel::createShaderProgram()
{
	// Shader creation is now handled by VulkanPipeline::getOrCreatePipeline()
	// This function is kept for compatibility but does nothing
	return true;
}

void UIPanel::updateGeometry(int drawableWidth, int drawableHeight)
{
	if (!vk_context_ || vk_vertex_buffer_ == VK_NULL_HANDLE)
	{
		return;
	}

	// X: SDL coords [0, width] -> NDC [-1, +1]
	float left = (static_cast<float>(x_) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	float right = (static_cast<float>(x_ + width_) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	
	// Y: With positive viewport, NDC Y=-1 is top, Y=+1 is bottom
	// SDL has Y=0 at top, so we map: pixel_y=0 -> NDC=-1, pixel_y=height -> NDC=+1
	float top = (static_cast<float>(y_) / static_cast<float>(drawableHeight)) * 2.0f - 1.0f;
	float bottom = (static_cast<float>(y_ + height_) / static_cast<float>(drawableHeight)) * 2.0f - 1.0f;
	
	first_draw_done_ = true;

	float vertices[] = {
		left, top, texcoord_left_, texcoord_top_,
		right, bottom, texcoord_right_, texcoord_bottom_,
		left, bottom, texcoord_left_, texcoord_bottom_,
		left, top, texcoord_left_, texcoord_top_,
		right, top, texcoord_right_, texcoord_top_,
		right, bottom, texcoord_right_, texcoord_bottom_
	};

	VkDevice device = vk_context_->getDevice();
	void* data = nullptr;
	if (vkMapMemory(device, vk_vertex_buffer_memory_, 0, sizeof(vertices), 0, &data) == VK_SUCCESS)
	{
		std::memcpy(data, vertices, sizeof(vertices));
		vkUnmapMemory(device, vk_vertex_buffer_memory_);
	}
}



void UIPanel::setVKContext(VKContext* context)
{
	vk_context_ = context;
	if (vk_context_)
	{
		createVulkanResources();
	}
}

void UIPanel::setRenderPass(VkRenderPass renderPass)
{
	vk_render_pass_ = renderPass;
	
	// If VKContext is already set but pipeline wasn't created (because renderPass was NULL before),
	// create the pipeline now
	if (vk_context_ && vk_render_pass_ != VK_NULL_HANDLE && (!shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE))
	{
		std::cout << "[UIPanel::setRenderPass] " << name_ << " - Creating pipeline with render pass" << std::endl;
		if (!createVulkanPipeline())
		{
			std::cout << "[UIPanel::setRenderPass] " << name_ << " - ERROR: Failed to create pipeline" << std::endl;
		}
		else
		{
			std::cout << "[UIPanel::setRenderPass] " << name_ << " - Pipeline created successfully" << std::endl;
		}
	}
}

void UIPanel::setVulkanPipelines(VulkanPipeline* pipelines)
{
	vulkan_pipelines_ = pipelines;
	std::cout << "[UIPanel::setVulkanPipelines] " << name_ << " - VulkanPipeline instance set" << std::endl;
}

bool UIPanel::createVulkanResources()
{
	if (!vk_context_)
	{
		return false;
	}

	if (!createTexture())
	{
		return false;
	}

	if (!createShaderProgram())
	{
		return false;
	}

	if (!createVulkanVertexBuffer())
	{
		return false;
	}

	if (!createVulkanDescriptorSet())
	{
		return false;
	}

	if (vk_render_pass_ != VK_NULL_HANDLE)
	{
		if (!createVulkanPipeline())
		{
			return false;
		}
	}

	return true;
}

void UIPanel::cleanupVulkanResources()
{
	if (!vk_context_)
	{
		return;
	}

	VkDevice device = vk_context_->getDevice();
	
	vkDeviceWaitIdle(device);

	if (vk_descriptor_pool_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, vk_descriptor_pool_, nullptr);
		vk_descriptor_pool_ = VK_NULL_HANDLE;
	}

	if (vk_descriptor_set_layout_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vk_descriptor_set_layout_ = VK_NULL_HANDLE;
	}

	// Release shared pipeline - VulkanPipeline will destroy when no longer referenced
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
}

bool UIPanel::createVulkanPipeline()
{
	if (!vk_context_ || !vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
	{
		std::cout << "[UIPanel::createVulkanPipeline] " << name_ << " - Missing VulkanPipeline or VKContext" << std::endl;
		return false;
	}

	if (vk_render_pass_ == VK_NULL_HANDLE)
	{
		std::cout << "[UIPanel::createVulkanPipeline] " << name_ << " - RenderPass is VK_NULL_HANDLE" << std::endl;
		return false;
	}

	if (vk_descriptor_set_layout_ == VK_NULL_HANDLE)
	{
		std::cout << "[UIPanel::createVulkanPipeline] " << name_ << " - DescriptorSetLayout not created yet" << std::endl;
		return false;
	}

	// GLSL Vertex Shader - Runtime compiled
	const std::string vertexShaderGLSL = R"(
	#version 450

	layout(location = 0) in vec2 aPosition;
	layout(location = 1) in vec2 aTexCoord;

	layout(location = 0) out vec2 vTexCoord;

	void main() {
		gl_Position = vec4(aPosition, 0.0, 1.0);
		vTexCoord = aTexCoord;
	}
	)";

	// GLSL Fragment Shader - Runtime compiled
	const std::string fragmentShaderGLSL = R"(
	#version 450

	layout(location = 0) in vec2 vTexCoord;
	layout(location = 0) out vec4 outColor;

	layout(binding = 0) uniform sampler2D texSampler;

	void main() {
		outColor = texture(texSampler, vTexCoord);
	}
	)";

	// Compile vertex shader
	std::cout << "[UIPanel] Compiling vertex shader for " << name_ << "..." << std::endl;
	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[UIPanel] Failed to compile vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Compile fragment shader
	std::cout << "[UIPanel] Compiling fragment shader for " << name_ << "..." << std::endl;
	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[UIPanel] Failed to compile fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Convert SPIR-V to byte vectors
	std::vector<char> vertShaderBytes = GLSLCompiler::spirvToBytes(vertexSPIRV);
	std::vector<char> fragShaderBytes = GLSLCompiler::spirvToBytes(fragmentSPIRV);

	// Configure vertex attributes
	VkVertexInputAttributeDescription attr0{};
	attr0.binding = 0;
	attr0.location = 0;
	attr0.format = VK_FORMAT_R32G32_SFLOAT;
	attr0.offset = 0;

	VkVertexInputAttributeDescription attr1{};
	attr1.binding = 0;
	attr1.location = 1;
	attr1.format = VK_FORMAT_R32G32_SFLOAT;
	attr1.offset = 2 * sizeof(float);

	std::vector<VkVertexInputAttributeDescription> vertexAttributes = { attr0, attr1 };

	// Build pipeline configuration
	VulkanPipeline::PipelineConfig config;
	config.name = "ui_panel_texture";
	config.vertexShaderCode = vertShaderBytes;
	config.fragmentShaderCode = fragShaderBytes;
	config.renderPass = vk_render_pass_;
	config.descriptorSetLayout = vk_descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 4 * sizeof(float);
	config.vertexAttributes = vertexAttributes;

	// Get or create pipeline from factory
	shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);
	
	if (!shared_pipeline_)
	{
		std::cout << "[UIPanel::createVulkanPipeline] " << name_ << " - Failed to get/create pipeline" << std::endl;
		return false;
	}

	std::cout << "[UIPanel::createVulkanPipeline] " << name_ << " - Pipeline acquired successfully (handle=" 
	          << shared_pipeline_->pipeline << ")" << std::endl;

	return true;
}

bool UIPanel::createVulkanVertexBuffer()
{
	if (!vk_context_)
	{
		return false;
	}

	VkDevice device = vk_context_->getDevice();

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(float) * 24;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &vk_vertex_buffer_) != VK_SUCCESS)
	{
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vk_vertex_buffer_, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &vk_vertex_buffer_memory_) != VK_SUCCESS)
	{
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_ = VK_NULL_HANDLE;
		return false;
	}

	vkBindBufferMemory(device, vk_vertex_buffer_, vk_vertex_buffer_memory_, 0);

	return true;
}

bool UIPanel::createVulkanDescriptorSet()
{
	if (!vk_context_)
	{
		return false;
	}

	VkDevice device = vk_context_->getDevice();

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &vk_descriptor_set_layout_) != VK_SUCCESS)
	{
		return false;
	}

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

	if (vkAllocateDescriptorSets(device, &allocInfo, &vk_descriptor_set_) != VK_SUCCESS)
	{
		return false;
	}

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = vk_texture_view_;
	imageInfo.sampler = vk_sampler_;

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = vk_descriptor_set_;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	bound_texture_view_ = vk_texture_view_;
	bound_sampler_ = vk_sampler_;

	return true;
}

uint32_t UIPanel::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	return 0;
}