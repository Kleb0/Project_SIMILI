#include "DataHolders.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include "json.hpp"
#include <iostream>
#include <mutex>
#include <cstring>
#include <algorithm>

using json = nlohmann::json;

DataHolders* DataHolders::s_instance_ = nullptr;

DataHolders::DataHolders()
{
}

DataHolders* DataHolders::getInstance()
{
	return s_instance_;
}

void DataHolders::setInstance(DataHolders* instance)
{
	s_instance_ = instance;
}

bool DataHolders::receiveDataHolders(const std::string& jsonBody)
{
	json requestData = json::parse(jsonBody, nullptr, false);
	if (requestData.is_discarded())
	{
		return false;
	}

	if (!requestData.contains("panel") || !requestData.contains("dataHolders") || !requestData["dataHolders"].is_array())
	{
		return false;
	}

	std::string panelName = requestData["panel"];
	std::vector<DataHolderEntry> entries;

	for (const auto& holder : requestData["dataHolders"])
	{
		if (holder.contains("field"))
		{
			DataHolderEntry e;
			e.field = holder["field"].get<std::string>();
			e.relX = holder.value("x", 0);
			e.relY = holder.value("y", 0);
			e.width = holder.value("w", 50);
			e.height = holder.value("h", 20);
			entries.push_back(std::move(e));
		}
	}

	float vpFracW = requestData.value("vpFracW", 0.0f);
	float vpFracH = requestData.value("vpFracH", 0.0f);

	int entryCount = static_cast<int>(entries.size());
	{
		std::lock_guard<std::mutex> lock(mutex_);
		panel_fields_[panelName] = std::move(entries);
		if (vpFracW > 0.0f && vpFracH > 0.0f)
			panel_viewport_fracs_[panelName] = { vpFracW, vpFracH };
	}

	std::cout << "[DataHolders] " << panelName << " -> Contains " << entryCount << " DataHolder(s)" << std::endl;

	return true;
}

int DataHolders::getCountForPanel(const std::string& panelName) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = panel_fields_.find(panelName);
	if (it != panel_fields_.end())
	{
		return static_cast<int>(it->second.size());
	}
	return 0;
}

bool DataHolders::initializeVulkan(VKContext* context, VulkanPipeline* pipelines, VkRenderPass renderPass)
{
	vk_context_ = context;
	vulkan_pipelines_ = pipelines;
	vk_render_pass_ = renderPass;

	const std::string vertSrc = R"(
	#version 450
	layout(location = 0) in vec2 inPos;
	void main() {
		gl_Position = vec4(inPos, 0.0, 1.0);
	}
	)";

		const std::string fragSrc = R"(
	#version 450
	layout(location = 0) out vec4 outColor;
	void main() {
		outColor = vec4(0.0, 1.0, 0.0, 1.0);
	}
	)";

	auto vertSpirv = GLSLCompiler::compileGLSL(vertSrc, GLSLCompiler::ShaderType::Vertex);
	if (vertSpirv.empty())
	{
		std::cerr << "[DataHolders] Vertex shader compile error: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	auto fragSpirv = GLSLCompiler::compileGLSL(fragSrc, GLSLCompiler::ShaderType::Fragment);
	if (fragSpirv.empty())
	{
		std::cerr << "[DataHolders] Fragment shader compile error: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	VulkanPipeline::PipelineConfig config;
	// Encode the renderPass handle in the name so each new renderPass
	// gets a fresh cache entry (avoids reusing stale pipeline after swapchain resize)
	config.name = "dataholder_green_overlay_" + std::to_string(reinterpret_cast<uintptr_t>(renderPass));
	config.vertexShaderCode = GLSLCompiler::spirvToBytes(vertSpirv);
	config.fragmentShaderCode = GLSLCompiler::spirvToBytes(fragSpirv);
	config.renderPass = renderPass;
	config.descriptorSetLayout = VK_NULL_HANDLE;
	config.enableBlending = false;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 2 * sizeof(float);

	VkVertexInputAttributeDescription posAttr{};
	posAttr.location = 0;
	posAttr.binding = 0;
	posAttr.format = VK_FORMAT_R32G32_SFLOAT;
	posAttr.offset = 0;
	config.vertexAttributes.push_back(posAttr);

	overlay_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);
	if (!overlay_pipeline_ || overlay_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[DataHolders] Failed to create green overlay pipeline" << std::endl;
		return false;
	}

	if (!createVertexBuffer())
	{
		return false;
	}

	vulkan_initialized_ = true;
	std::cout << "[DataHolders] Vulkan green overlay pipeline initialized" << std::endl;
	return true;
}

bool DataHolders::createVertexBuffer()
{
	VkDeviceSize bufferSize = static_cast<VkDeviceSize>(kMaxOverlays * 6 * 2 * sizeof(float));

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(vk_context_->getDevice(), &bufferInfo, nullptr, &vertex_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[DataHolders] Failed to create overlay vertex buffer" << std::endl;
		return false;
	}

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(vk_context_->getDevice(), vertex_buffer_, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(vk_context_->getDevice(), &allocInfo, nullptr, &vertex_buffer_memory_) != VK_SUCCESS)
	{
		std::cerr << "[DataHolders] Failed to allocate overlay vertex buffer memory" << std::endl;
		vkDestroyBuffer(vk_context_->getDevice(), vertex_buffer_, nullptr);
		vertex_buffer_ = VK_NULL_HANDLE;
		return false;
	}

	vkBindBufferMemory(vk_context_->getDevice(), vertex_buffer_, vertex_buffer_memory_, 0);
	return true;
}

uint32_t DataHolders::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	return 0;
}

void DataHolders::drawForPanel(
	const std::string& panelName,
	int panelDrawX, int panelDrawY, int panelDrawW, int panelDrawH,
	int logicalW, int logicalH,
	VkCommandBuffer cmdBuffer, int drawableW, int drawableH,
	VKContext* vkContext, VulkanPipeline* vkPipelines, VkRenderPass renderPass)
{
	if (!vkContext || !vkPipelines || renderPass == VK_NULL_HANDLE) return;
	if (cmdBuffer == VK_NULL_HANDLE) return;
	if (drawableW <= 0 || drawableH <= 0) return;
	if (panelDrawW <= 0 || panelDrawH <= 0) return;

	if (!vulkan_initialized_)
	{
		if (!initializeVulkan(vkContext, vkPipelines, renderPass)) return;
	}
	else if (vk_render_pass_ != renderPass)
	{
		// RenderPass was recreated (e.g. window resize): rebuild pipeline
		cleanupVulkan();
		if (!initializeVulkan(vkContext, vkPipelines, renderPass)) return;
	}
	else if (!overlay_pipeline_ || overlay_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		cleanupVulkan();
		if (!initializeVulkan(vkContext, vkPipelines, renderPass)) return;
	}

	std::vector<DataHolderEntry> entries;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = panel_fields_.find(panelName);
		if (it == panel_fields_.end() || it->second.empty()) return;
		entries = it->second;
	}

	int count = std::min(static_cast<int>(entries.size()), kMaxOverlays);
	float scaleX = (logicalW > 0) ? static_cast<float>(panelDrawW) / logicalW : 1.0f;
	float scaleY = (logicalH > 0) ? static_cast<float>(panelDrawH) / logicalH : 1.0f;

	// Viewport dimensions (CSS content area, excludes scrollbar)
	float vpFracW = 1.0f, vpFracH = 1.0f;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto vit = panel_viewport_fracs_.find(panelName);
		if (vit != panel_viewport_fracs_.end())
		{
			vpFracW = vit->second.first;
			vpFracH = vit->second.second;
		}
	}
	// Scissor uses the stable ratio: adapts automatically to any panel resize
	// Clamp to framebuffer bounds (Vulkan requires offset + extent <= framebuffer size)
	int scissorX = std::max(0, panelDrawX);
	int scissorY = std::max(0, panelDrawY);
	int scissorW = std::max(1, std::min(static_cast<int>(vpFracW * panelDrawW), drawableW - scissorX));
	int scissorH = std::max(1, std::min(static_cast<int>(vpFracH * panelDrawH), drawableH - scissorY));

	std::vector<float> vertices;
	vertices.reserve(count * 6 * 2);

	for (int i = 0; i < count; i++)
	{
		const auto& e = entries[i];

		float ax = panelDrawX + e.relX * scaleX;
		float ay = panelDrawY + e.relY * scaleY;
		float bx = ax + e.width  * scaleX;
		float by = ay + e.height * scaleY;

		float ndcX0 = 2.0f * ax / drawableW - 1.0f;
		float ndcY0 = 2.0f * ay / drawableH - 1.0f;
		float ndcX1 = 2.0f * bx / drawableW - 1.0f;
		float ndcY1 = 2.0f * by / drawableH - 1.0f;

		// Triangle 1: TL, TR, BL
		vertices.push_back(ndcX0); vertices.push_back(ndcY0);
		vertices.push_back(ndcX1); vertices.push_back(ndcY0);
		vertices.push_back(ndcX0); vertices.push_back(ndcY1);
		// Triangle 2: TR, BR, BL
		vertices.push_back(ndcX1); vertices.push_back(ndcY0);
		vertices.push_back(ndcX1); vertices.push_back(ndcY1);
		vertices.push_back(ndcX0); vertices.push_back(ndcY1);
	}

	void* data;
	vkMapMemory(vkContext->getDevice(), vertex_buffer_memory_, 0, vertices.size() * sizeof(float), 0, &data);
	std::memcpy(data, vertices.data(), vertices.size() * sizeof(float));
	vkUnmapMemory(vkContext->getDevice(), vertex_buffer_memory_);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlay_pipeline_->pipeline);

	// Clip rendering to the panel content bounds (excludes scrollbar area)
	VkRect2D scissor{};
	scissor.offset = { scissorX, scissorY };
	scissor.extent = { static_cast<uint32_t>(scissorW), static_cast<uint32_t>(scissorH) };
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

	VkBuffer buf = vertex_buffer_;
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buf, &offset);
	vkCmdDraw(cmdBuffer, count * 6, 1, 0, 0);

	// Restore scissor to full drawable area
	VkRect2D fullScissor{};
	fullScissor.offset = { 0, 0 };
	fullScissor.extent = { static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH) };
	vkCmdSetScissor(cmdBuffer, 0, 1, &fullScissor);
}

void DataHolders::cleanupVulkan()
{
	if (vk_context_)
	{
		VkDevice device = vk_context_->getDevice();
		if (vertex_buffer_ != VK_NULL_HANDLE)
		{
			vkDestroyBuffer(device, vertex_buffer_, nullptr);
			vertex_buffer_ = VK_NULL_HANDLE;
		}
		if (vertex_buffer_memory_ != VK_NULL_HANDLE)
		{
			vkFreeMemory(device, vertex_buffer_memory_, nullptr);
			vertex_buffer_memory_ = VK_NULL_HANDLE;
		}
	}
	overlay_pipeline_ = nullptr;
	vulkan_initialized_ = false;
}
