#include "Splitter.hpp"
#include "SplitterMouseMecanic.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

Splitter::Splitter()
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
	, hovered_index_(-1)
	, splitter_mouse_mecanic_(nullptr)
{
}

Splitter::~Splitter()
{
	shutdown();
}

bool Splitter::initialize(VKContext* vkContext, VkRenderPass renderPass)
{
	if (!vkContext || renderPass == VK_NULL_HANDLE)
	{
		std::cerr << "[Splitter] Invalid parameters for initialization" << std::endl;
		return false;
	}

	vk_context_ = vkContext;
	vk_render_pass_ = renderPass;

	if (!createGraphicsResources())
	{
		std::cerr << "[Splitter] Failed to create graphics resources" << std::endl;
		return false;
	}

	initialized_ = true;
	std::cout << "[Splitter] Initialized successfully" << std::endl;
	return true;
}

void Splitter::shutdown()
{
	destroyGraphicsResources();

	splitters_.clear();
	initialized_ = false;
}

void Splitter::setVulkanPipelines(VulkanPipeline* pipelines)
{
	vulkan_pipelines_ = pipelines;
}

void Splitter::setHoveredIndex(int index)
{
	hovered_index_ = index;
}

void Splitter::setSplitterMouseMecanic(SplitterMouseMecanic* mecanic)
{
	splitter_mouse_mecanic_ = mecanic;
}

void Splitter::setSplitters(const std::vector<SplitterData>& splitters)
{
	splitters_ = splitters;
}

void Splitter::draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight)
{
	if (!initialized_ || !vk_context_ || shared_pipeline_.get() == nullptr || shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		return;
	}

	if (splitters_.empty() || drawableWidth <= 0 || drawableHeight <= 0)
	{
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
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->layout, 0, 1, &vk_descriptor_set_, 0, nullptr);

	const uint32_t count = static_cast<uint32_t>(splitters_.size());
	std::vector<float> allVertices;
	allVertices.reserve(count * 12);

	for (const auto& splitter : splitters_)
	{
		const float ndcX1 = (2.0f * static_cast<float>(splitter.x) / static_cast<float>(drawableWidth)) - 1.0f;
		const float ndcY1 = (2.0f * static_cast<float>(splitter.y) / static_cast<float>(drawableHeight)) - 1.0f;
		const float ndcX2 = (2.0f * static_cast<float>(splitter.x + splitter.width) / static_cast<float>(drawableWidth)) - 1.0f;
		const float ndcY2 = (2.0f * static_cast<float>(splitter.y + splitter.height) / static_cast<float>(drawableHeight)) - 1.0f;

		allVertices.insert(allVertices.end(), {
			ndcX1, ndcY1,
			ndcX2, ndcY1,
			ndcX1, ndcY2,
			ndcX2, ndcY1,
			ndcX2, ndcY2,
			ndcX1, ndcY2
		});
	}

	VkDevice device = vk_context_->getDevice();
	void* data = nullptr;
	vkMapMemory(device, vk_vertex_buffer_memory_, 0, sizeof(float) * allVertices.size(), 0, &data);
	std::memcpy(data, allVertices.data(), sizeof(float) * allVertices.size());
	vkUnmapMemory(device, vk_vertex_buffer_memory_);

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vk_vertex_buffer_, offsets);

	// ----------------------- here we change the color for hovered splitter
	for (uint32_t i = 0; i < count; ++i)
	{
		float color[4];
		const bool isActive = splitter_mouse_mecanic_ &&
			(static_cast<int>(i) == hovered_index_ || static_cast<int>(i) == splitter_mouse_mecanic_->getDraggedIndex());
		if (isActive)
		{
			SplitterMouseMecanic::Color c = splitter_mouse_mecanic_->getHoverColor();
			color[0] = c.r; color[1] = c.g; color[2] = c.b; color[3] = c.a;
		}
		else if (splitter_mouse_mecanic_)
		{
			SplitterMouseMecanic::Color c = splitter_mouse_mecanic_->getDefaultColor();
			color[0] = c.r; color[1] = c.g; color[2] = c.b; color[3] = c.a;
		}
		else
		{
			color[0] = 0.0f; color[1] = 0.0f; color[2] = 0.0f; color[3] = 0.0f;
		}
		vkCmdPushConstants(commandBuffer, shared_pipeline_->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(color), color);
		vkCmdDraw(commandBuffer, 6, 1, i * 6, 0);
	}
}

void Splitter::drawSplitter(VkCommandBuffer commandBuffer, const SplitterData& splitter, int drawableWidth, int drawableHeight)
{
	if (!vk_context_)
	{
		return;
	}

	const float ndcX1 = (2.0f * static_cast<float>(splitter.x) / static_cast<float>(drawableWidth)) - 1.0f;
	const float ndcY1 = (2.0f * static_cast<float>(splitter.y) / static_cast<float>(drawableHeight)) - 1.0f;
	const float ndcX2 = (2.0f * static_cast<float>(splitter.x + splitter.width) / static_cast<float>(drawableWidth)) - 1.0f;
	const float ndcY2 = (2.0f * static_cast<float>(splitter.y + splitter.height) / static_cast<float>(drawableHeight)) - 1.0f;

	const float vertices[] = {
		ndcX1, ndcY1,
		ndcX2, ndcY1,
		ndcX1, ndcY2,
		ndcX2, ndcY1,
		ndcX2, ndcY2,
		ndcX1, ndcY2
	};

	VkDevice device = vk_context_->getDevice();

	void* data = nullptr;
	vkMapMemory(device, vk_vertex_buffer_memory_, 0, sizeof(vertices), 0, &data);
	std::memcpy(data, vertices, sizeof(vertices));
	vkUnmapMemory(device, vk_vertex_buffer_memory_);

	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vk_vertex_buffer_, offsets);

	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

bool Splitter::createGraphicsResources()
{
	if (!vk_context_ || !vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
	{
		std::cerr << "[Splitter] Missing VulkanPipeline or VKContext" << std::endl;
		return false;
	}

	if (vk_render_pass_ == VK_NULL_HANDLE)
	{
		std::cerr << "[Splitter] RenderPass is VK_NULL_HANDLE" << std::endl;
		return false;
	}

	VkDevice device = vk_context_->getDevice();

	const std::string vertexShaderGLSL = R"(
	#version 450

	layout(location = 0) in vec2 inPosition;

	void main() {
		gl_Position = vec4(inPosition, 0.0, 1.0);
	}
	)";

	const std::string fragmentShaderGLSL = R"(
	#version 450

	layout(push_constant) uniform PushConstants {
		vec4 color;
	} pc;

	layout(location = 0) out vec4 outColor;

	void main() {
		outColor = pc.color;
	}
	)";

	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[Splitter] Failed to compile vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[Splitter] Failed to compile fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	std::vector<char> vertShaderBytes = GLSLCompiler::spirvToBytes(vertexSPIRV);
	std::vector<char> fragShaderBytes = GLSLCompiler::spirvToBytes(fragmentSPIRV);

	static constexpr uint32_t MAX_SPLITTERS = 32;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(float) * 12 * MAX_SPLITTERS;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &vk_vertex_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[Splitter] Failed to create vertex buffer" << std::endl;
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
		std::cerr << "[Splitter] Failed to allocate vertex buffer memory" << std::endl;
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
		std::cerr << "[Splitter] Failed to create descriptor set layout" << std::endl;
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_memory_ = VK_NULL_HANDLE;
		vk_vertex_buffer_ = VK_NULL_HANDLE;
		return false;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = 1;
	imageInfo.extent.height = 1;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

	if (vkCreateImage(device, &imageInfo, nullptr, &dummy_texture_image_) != VK_SUCCESS)
	{
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, dummy_texture_image_, &memReqs);

	VkMemoryAllocateInfo allocInfo2{};
	allocInfo2.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo2.allocationSize = memReqs.size;
	allocInfo2.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(device, &allocInfo2, nullptr, &dummy_texture_memory_) != VK_SUCCESS)
	{
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
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
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
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

	if (vkCreateSampler(device, &samplerInfo, nullptr, &dummy_texture_sampler_) != VK_SUCCESS)
	{
		vkDestroyImageView(device, dummy_texture_view_, nullptr);
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
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
		vkDestroySampler(device, dummy_texture_sampler_, nullptr);
		vkDestroyImageView(device, dummy_texture_view_, nullptr);
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	VkDescriptorSetAllocateInfo allocInfoDesc{};
	allocInfoDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfoDesc.descriptorPool = vk_descriptor_pool_;
	allocInfoDesc.descriptorSetCount = 1;
	allocInfoDesc.pSetLayouts = &vk_descriptor_set_layout_;

	if (vkAllocateDescriptorSets(device, &allocInfoDesc, &vk_descriptor_set_) != VK_SUCCESS)
	{
		vkDestroyDescriptorPool(device, vk_descriptor_pool_, nullptr);
		vkDestroySampler(device, dummy_texture_sampler_, nullptr);
		vkDestroyImageView(device, dummy_texture_view_, nullptr);
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	VkDescriptorImageInfo imageDescInfo{};
	imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
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

	VulkanPipeline::PipelineConfig config;
	config.name = "splitter_color";
	config.vertexShaderCode = vertShaderBytes;
	config.fragmentShaderCode = fragShaderBytes;
	config.renderPass = vk_render_pass_;
	config.descriptorSetLayout = vk_descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 2 * sizeof(float);
	config.vertexAttributes = vertexAttributes;

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(float) * 4;
	config.pushConstantRanges = { pushRange };

	shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);
	
	if (shared_pipeline_.get() == nullptr)
	{
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_memory_ = VK_NULL_HANDLE;
		vk_vertex_buffer_ = VK_NULL_HANDLE;
		std::cerr << "[Splitter] Failed to get/create pipeline" << std::endl;
		return false;
	}

	std::cout << "[Splitter] Graphics resources created successfully" << std::endl;
	return true;
}

void Splitter::destroyGraphicsResources()
{
	if (!vk_context_)
	{
		return;
	}

	VkDevice device = vk_context_->getDevice();
	
	vkDeviceWaitIdle(device);

	shared_pipeline_ = nullptr;

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
}

uint32_t Splitter::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

	std::cerr << "[Splitter] Failed to find suitable memory type" << std::endl;
	return 0;
}
