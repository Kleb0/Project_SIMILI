#include "Border_DebugRectangle.hpp"
#include "../App_Border.hpp"
#include "../Engine/VulkanScene/VKcontext.hpp"
#include "../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "../Engine/GLSL_Compiler/GLSLCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

Border_DebugRectangle::Border_DebugRectangle()
	: vk_context_(nullptr)
	, vulkan_pipelines_(nullptr)
	, vk_render_pass_(VK_NULL_HANDLE)
	, initialized_(false)
	, vk_vertex_buffer_(VK_NULL_HANDLE)
	, vk_vertex_buffer_memory_(VK_NULL_HANDLE)
	, vk_descriptor_set_layout_(VK_NULL_HANDLE)
	, vk_descriptor_pool_(VK_NULL_HANDLE)
	, vk_descriptor_set_(VK_NULL_HANDLE)
	, dummy_texture_image_(VK_NULL_HANDLE)
	, dummy_texture_memory_(VK_NULL_HANDLE)
	, dummy_texture_view_(VK_NULL_HANDLE)
	, dummy_texture_sampler_(VK_NULL_HANDLE)
	, shared_pipeline_(nullptr)
{
}

Border_DebugRectangle::~Border_DebugRectangle()
{
	shutdown();
}

bool Border_DebugRectangle::initialize(VKContext* vkContext, VkRenderPass renderPass)
{
	std::cout << "[Border_DebugRectangle] initialize() called" << std::endl;
	
	if (initialized_)
	{
		std::cout << "[Border_DebugRectangle] Already initialized" << std::endl;
		return true;
	}

	vk_context_ = vkContext;
	vk_render_pass_ = renderPass;

	if (!createGraphicsResources())
	{
		std::cerr << "[Border_DebugRectangle] Failed to create graphics resources" << std::endl;
		return false;
	}

	initialized_ = true;
	std::cout << "[Border_DebugRectangle] Successfully initialized" << std::endl;
	return true;
}

void Border_DebugRectangle::shutdown()
{
	destroyGraphicsResources();
	initialized_ = false;
}

void Border_DebugRectangle::setVulkanPipelines(VulkanPipeline* pipelines)
{
	vulkan_pipelines_ = pipelines;
}

void Border_DebugRectangle::draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, App_Border* border)
{

	if (!initialized_ || !border || !border->isDebugLineEnabled())
	{
		return;
	}

	if (!shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[Border_DebugRectangle] draw() - Pipeline is null!" << std::endl;
		return;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->pipeline);

	VkBuffer vertexBuffers[] = { vk_vertex_buffer_ };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	if (vk_descriptor_set_ != VK_NULL_HANDLE && shared_pipeline_->layout != VK_NULL_HANDLE)
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->layout, 0, 1, &vk_descriptor_set_, 0, nullptr);
	}

	int left = border->getLeft();
	int top = border->getTop();
	int right = border->getRight();
	int bottom = border->getBottom();

	int refWidth = border->getReferenceWindowWidth();
	int refHeight = border->getReferenceWindowHeight();

	if (refWidth <= 0 || refHeight <= 0)
	{
		return;
	}

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(refWidth);
	viewport.height = static_cast<float>(refHeight);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = {static_cast<uint32_t>(refWidth), static_cast<uint32_t>(refHeight)};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	const int thickness = 5;

	float nx_left = (left / (float)refWidth) * 2.0f - 1.0f;
	float ny_top = (top / (float)refHeight) * 2.0f - 1.0f;
	float nx_right = (right / (float)refWidth) * 2.0f - 1.0f;
	float ny_bottom = (bottom / (float)refHeight) * 2.0f - 1.0f;

	float thickness_x = (thickness / (float)refWidth) * 2.0f;
	float thickness_y = (thickness / (float)refHeight) * 2.0f;

	std::vector<float> vertices;
	vertices.reserve(48);

	vertices.push_back(nx_left);
	vertices.push_back(ny_top - thickness_y);
	vertices.push_back(nx_left);
	vertices.push_back(ny_top + thickness_y);
	vertices.push_back(nx_right);
	vertices.push_back(ny_top - thickness_y);

	vertices.push_back(nx_left);
	vertices.push_back(ny_top + thickness_y);
	vertices.push_back(nx_right);
	vertices.push_back(ny_top + thickness_y);
	vertices.push_back(nx_right);
	vertices.push_back(ny_top - thickness_y);

	vertices.push_back(nx_right - thickness_x);
	vertices.push_back(ny_top);
	vertices.push_back(nx_right + thickness_x);
	vertices.push_back(ny_top);
	vertices.push_back(nx_right - thickness_x);
	vertices.push_back(ny_bottom);

	vertices.push_back(nx_right + thickness_x);
	vertices.push_back(ny_top);
	vertices.push_back(nx_right + thickness_x);
	vertices.push_back(ny_bottom);
	vertices.push_back(nx_right - thickness_x);
	vertices.push_back(ny_bottom);

	vertices.push_back(nx_right);
	vertices.push_back(ny_bottom - thickness_y);
	vertices.push_back(nx_right);
	vertices.push_back(ny_bottom + thickness_y);
	vertices.push_back(nx_left);
	vertices.push_back(ny_bottom - thickness_y);

	vertices.push_back(nx_right);
	vertices.push_back(ny_bottom + thickness_y);
	vertices.push_back(nx_left);
	vertices.push_back(ny_bottom + thickness_y);
	vertices.push_back(nx_left);
	vertices.push_back(ny_bottom - thickness_y);

	vertices.push_back(nx_left - thickness_x);
	vertices.push_back(ny_bottom);
	vertices.push_back(nx_left + thickness_x);
	vertices.push_back(ny_bottom);
	vertices.push_back(nx_left - thickness_x);
	vertices.push_back(ny_top);

	vertices.push_back(nx_left + thickness_x);
	vertices.push_back(ny_bottom);
	vertices.push_back(nx_left + thickness_x);
	vertices.push_back(ny_top);
	vertices.push_back(nx_left - thickness_x);
	vertices.push_back(ny_top);

	VkDevice device = vk_context_->getDevice();
	void* data;
	if (vkMapMemory(device, vk_vertex_buffer_memory_, 0, vertices.size() * sizeof(float), 0, &data) == VK_SUCCESS)
	{
		memcpy(data, vertices.data(), vertices.size() * sizeof(float));
		vkUnmapMemory(device, vk_vertex_buffer_memory_);
	}

	vkCmdDraw(commandBuffer, 24, 1, 0, 0);
	VkViewport fullViewport{};
	fullViewport.x = 0.0f;
	fullViewport.y = 0.0f;
	fullViewport.width = static_cast<float>(drawableWidth);
	fullViewport.height = static_cast<float>(drawableHeight);
	fullViewport.minDepth = 0.0f;
	fullViewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &fullViewport);

	VkRect2D fullScissor{};
	fullScissor.offset = {0, 0};
	fullScissor.extent = {static_cast<uint32_t>(drawableWidth), static_cast<uint32_t>(drawableHeight)};
	vkCmdSetScissor(commandBuffer, 0, 1, &fullScissor);

}

void Border_DebugRectangle::drawLine(VkCommandBuffer commandBuffer, int x1, int y1, int x2, int y2, int thickness, int drawableWidth, int drawableHeight)
{
	float nx1 = (x1 / (float)drawableWidth) * 2.0f - 1.0f;
	float ny1 = (y1 / (float)drawableHeight) * 2.0f - 1.0f;
	float nx2 = (x2 / (float)drawableWidth) * 2.0f - 1.0f;
	float ny2 = (y2 / (float)drawableHeight) * 2.0f - 1.0f;

	float dx = nx2 - nx1;
	float dy = ny2 - ny1;
	float length = std::sqrt(dx * dx + dy * dy);

	if (length < 0.0001f)
	{
		return;
	}

	float perpX = -dy / length;
	float perpY = dx / length;

	float halfThickness = (thickness / (float)drawableHeight);
	perpX *= halfThickness;
	perpY *= halfThickness;

	float vertices[12] = 
	{
		nx1 - perpX, ny1 - perpY,
		nx1 + perpX, ny1 + perpY,
		nx2 - perpX, ny2 - perpY,

		nx1 + perpX, ny1 + perpY,
		nx2 + perpX, ny2 + perpY,
		nx2 - perpX, ny2 - perpY
	};

	void* data;
	VkDevice device = vk_context_->getDevice();

	if (vkMapMemory(device, vk_vertex_buffer_memory_, 0, sizeof(vertices), 0, &data) == VK_SUCCESS)
	{
		memcpy(data, vertices, sizeof(vertices));
		vkUnmapMemory(device, vk_vertex_buffer_memory_);
	}

	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

bool Border_DebugRectangle::createGraphicsResources()
{
	std::cout << "[Border_DebugRectangle] createGraphicsResources() started" << std::endl;
	
	if (!vk_context_ || !vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
	{
		std::cerr << "[Border_DebugRectangle] Missing VulkanPipeline or VKContext" << std::endl;
		std::cerr << "  vk_context_=" << (vk_context_ != nullptr) 
		          << " vulkan_pipelines_=" << (vulkan_pipelines_ != nullptr);
		if (vulkan_pipelines_)
		{
			std::cerr << " isInitialized=" << vulkan_pipelines_->isInitialized();
		}
		std::cerr << std::endl;
		return false;
	}

	if (vk_render_pass_ == VK_NULL_HANDLE)
	{
		std::cerr << "[Border_DebugRectangle] RenderPass is VK_NULL_HANDLE" << std::endl;
		return false;
	}

	VkDevice device = vk_context_->getDevice();
	std::cout << "[Border_DebugRectangle] VkDevice obtained" << std::endl;

	const std::string vertexShaderGLSL = R"(
	#version 450

	layout(location = 0) in vec2 inPosition;

	void main() {
		gl_Position = vec4(inPosition, 0.0, 1.0);
	}
	)";

	const std::string fragmentShaderGLSL = R"(
	#version 450

	layout(location = 0) out vec4 outColor;

	void main() {
		outColor = vec4(1.0, 0.0, 0.0, 1.0);
	}
	)";

	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[Border_DebugRectangle] Failed to compile vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[Border_DebugRectangle] Failed to compile fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	std::vector<char> vertShaderBytes = GLSLCompiler::spirvToBytes(vertexSPIRV);
	std::vector<char> fragShaderBytes = GLSLCompiler::spirvToBytes(fragmentSPIRV);

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(float) * 48;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &vk_vertex_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to create vertex buffer" << std::endl;
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, vk_vertex_buffer_, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &vk_vertex_buffer_memory_) != VK_SUCCESS)
	{
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_ = VK_NULL_HANDLE;
		std::cerr << "[Border_DebugRectangle] Failed to allocate vertex buffer memory" << std::endl;
		return false;
	}

	vkBindBufferMemory(device, vk_vertex_buffer_, vk_vertex_buffer_memory_, 0);

	VkVertexInputAttributeDescription attr0{};
	attr0.binding = 0;
	attr0.location = 0;
	attr0.format = VK_FORMAT_R32G32_SFLOAT;
	attr0.offset = 0;

	std::vector<VkVertexInputAttributeDescription> vertexAttributes = { attr0 };

	VkDescriptorSetLayoutBinding dummyBinding{};
	dummyBinding.binding = 0;
	dummyBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	dummyBinding.descriptorCount = 1;
	dummyBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	dummyBinding.pImmutableSamplers = nullptr;
	
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &dummyBinding;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &vk_descriptor_set_layout_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to create descriptor set layout" << std::endl;
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
		std::cerr << "[Border_DebugRectangle] Failed to create descriptor pool" << std::endl;
		return false;
	}

	VkDescriptorSetAllocateInfo allocSetInfo{};
	allocSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocSetInfo.descriptorPool = vk_descriptor_pool_;
	allocSetInfo.descriptorSetCount = 1;
	allocSetInfo.pSetLayouts = &vk_descriptor_set_layout_;

	if (vkAllocateDescriptorSets(device, &allocSetInfo, &vk_descriptor_set_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to allocate descriptor set" << std::endl;
		return false;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.width = 1;
	imageInfo.extent.height = 1;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if (vkCreateImage(device, &imageInfo, nullptr, &dummy_texture_image_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to create dummy texture image" << std::endl;
		return false;
	}

	VkMemoryRequirements imgMemReqs;
	vkGetImageMemoryRequirements(device, dummy_texture_image_, &imgMemReqs);

	VkMemoryAllocateInfo imgAllocInfo{};
	imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	imgAllocInfo.allocationSize = imgMemReqs.size;
	imgAllocInfo.memoryTypeIndex = findMemoryType(imgMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &dummy_texture_memory_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to allocate dummy texture memory" << std::endl;
		return false;
	}

	vkBindImageMemory(device, dummy_texture_image_, dummy_texture_memory_, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = dummy_texture_image_;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &viewInfo, nullptr, &dummy_texture_view_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to create dummy texture view" << std::endl;
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

	if (vkCreateSampler(device, &samplerInfo, nullptr, &dummy_texture_sampler_) != VK_SUCCESS)
	{
		std::cerr << "[Border_DebugRectangle] Failed to create sampler" << std::endl;
		return false;
	}

	VkDescriptorImageInfo imageDescInfo{};
	imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageDescInfo.imageView = dummy_texture_view_;
	imageDescInfo.sampler = dummy_texture_sampler_;

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = vk_descriptor_set_;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageDescInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

	std::cout << "[Border_DebugRectangle] Creating pipeline..." << std::endl;
	
	VulkanPipeline::PipelineConfig config;
	config.name = "border_debug_rectangle";
	config.vertexShaderCode = vertShaderBytes;
	config.fragmentShaderCode = fragShaderBytes;
	config.renderPass = vk_render_pass_;
	config.descriptorSetLayout = vk_descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 2 * sizeof(float);
	config.vertexAttributes = vertexAttributes;

	shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);

	if (!shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[Border_DebugRectangle] Failed to create pipeline" << std::endl;
		return false;
	}

	std::cout << "[Border_DebugRectangle] Pipeline created successfully!" << std::endl;
	std::cout << "[Border_DebugRectangle] createGraphicsResources() completed" << std::endl;
	
	return true;
}

void Border_DebugRectangle::destroyGraphicsResources()
{
	if (!vk_context_)
	{
		return;
	}

	VkDevice device = vk_context_->getDevice();

	if (vk_vertex_buffer_ != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_ = VK_NULL_HANDLE;
	}

	if (vk_vertex_buffer_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vk_vertex_buffer_memory_ = VK_NULL_HANDLE;
	}

	if (dummy_texture_sampler_ != VK_NULL_HANDLE)
	{
		vkDestroySampler(device, dummy_texture_sampler_, nullptr);
		dummy_texture_sampler_ = VK_NULL_HANDLE;
	}

	if (dummy_texture_view_ != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, dummy_texture_view_, nullptr);
		dummy_texture_view_ = VK_NULL_HANDLE;
	}

	if (dummy_texture_image_ != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		dummy_texture_image_ = VK_NULL_HANDLE;
	}

	if (dummy_texture_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		dummy_texture_memory_ = VK_NULL_HANDLE;
	}

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
}

uint32_t Border_DebugRectangle::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDevice physicalDevice = vk_context_->getPhysicalDevice();
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	return 0;
}
