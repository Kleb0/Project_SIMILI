#include "Splitter.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>

namespace
{
	constexpr int kDefaultSplitterThickness = 8;
	constexpr int kMinPanelWidth = 120;
	constexpr int kMinPanelHeight = 80;
}

Splitter::Splitter()
	: window_(nullptr)
	, vk_context_(nullptr)
	, vulkan_pipelines_(nullptr)
	, vk_render_pass_(VK_NULL_HANDLE)
	, initialized_(false)
	, layout_ready_(false)
	, dragging_(false)
	, active_splitter_index_(-1)
	, hovered_splitter_index_(-1)
	, drag_anchor_(0)
	, last_window_width_(0)
	, last_window_height_(0)
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

Splitter::~Splitter()
{
	shutdown();
}

bool Splitter::initialize(SDL_Window* window, VKContext* vkContext, VkRenderPass renderPass)
{
	if (initialized_)
	{
		window_ = window;
		vk_context_ = vkContext;
		vk_render_pass_ = renderPass;
		return true;
	}

	if (!window || !vkContext || renderPass == VK_NULL_HANDLE)
	{
		return false;
	}

	window_ = window;
	vk_context_ = vkContext;
	vk_render_pass_ = renderPass;

	SDL_GetWindowSize(window_, &last_window_width_, &last_window_height_);
	initialized_ = true;
	return true;
}

bool Splitter::finalizeInitialization()
{
	if (!initialized_)
	{
		std::cerr << "[Splitter] Cannot finalize: not initialized" << std::endl;
		return false;
	}

	if (!createGraphicsResources())
	{
		std::cerr << "[Splitter] Failed to create graphics resources" << std::endl;
		std::cout << "--------------- [Splitter] Finalize initialization failed------------ \n " << std::endl;
		shutdown();
		return false;
	}

	std::cout << "[Splitter] Graphics resources created successfully" << std::endl;
	return true;
}

void Splitter::shutdown()
{
	destroyGraphicsResources();

	std::lock_guard<std::mutex> lock(splitter_mutex_);
	panel_state_map_.clear();
	source_frame_data_map_.clear();
	splitters_.clear();
	window_ = nullptr;
	initialized_ = false;
	layout_ready_ = false;
	dragging_ = false;
	active_splitter_index_ = -1;
	hovered_splitter_index_ = -1;
	drag_anchor_ = 0;
	last_window_width_ = 0;
	last_window_height_ = 0;
}

bool Splitter::isReady() const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	return initialized_ && layout_ready_;
}

void Splitter::setVulkanPipelines(VulkanPipeline* pipelines)
{
	vulkan_pipelines_ = pipelines;
	std::cout << "[Splitter::setVulkanPipelines] VulkanPipeline instance set" << std::endl;
}

void Splitter::forceRefreshLayout()
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	
	if (!window_)
	{
		return;
	}
	
	int windowWidth = 0;
	int windowHeight = 0;
	SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
	
	std::cout << "[Splitter::forceRefreshLayout] Layout refresh forced at window size: " 
	          << windowWidth << "x" << windowHeight << std::endl;
	
	if (layout_ready_ && !panel_state_map_.empty() && windowWidth > 0 && windowHeight > 0 &&
	    (windowWidth != last_window_width_ || windowHeight != last_window_height_))
	{
		std::cout << "[Splitter::forceRefreshLayout] Window size changed from " 
		          << last_window_width_ << "x" << last_window_height_ 
		          << " to " << windowWidth << "x" << windowHeight 
		          << " - scaling layout immediately" << std::endl;
		scaleLayoutToWindow(windowWidth, windowHeight);
		refreshDerivedData();
	}
}

void Splitter::syncFrameDatas(const SIMILI::Frontend::FrameDatas* frameDatas)
{
	std::cout << "\n---------------- [Splitter] syncFrameDatas START ----------------" << std::endl;

	if (!initialized_)
	{
		std::cout << "[Splitter] ERROR: Not initialized, returning early" << std::endl;
		std::cout << "---------------- [Splitter] syncFrameDatas END ----------------\n" << std::endl;
		return;
	}

	if (!frameDatas)
	{
		std::cout << "[Splitter] ERROR: frameDatas is null, returning early" << std::endl;
		std::cout << "---------------- [Splitter] syncFrameDatas END ----------------\n" << std::endl;
		return;
	}

	const auto& frameDataMap = frameDatas->getFrameData();
	std::cout << "[Splitter] FrameDataMap size: " << frameDataMap.size() << std::endl;
	
	if (frameDataMap.empty())
	{
		std::cout << "[Splitter] ERROR: frameDataMap is empty, returning early" << std::endl;
		std::cout << "---------------- [Splitter] syncFrameDatas END ----------------\n" << std::endl;
		return;
	}

	for (const auto& pair : frameDataMap)
	{
		std::cout << "[Splitter] Frame data: " << pair.first 
				  << " - X:" << pair.second.relativeX 
				  << " Y:" << pair.second.relativeY 
				  << " W:" << pair.second.width 
				  << " H:" << pair.second.height << std::endl;
	}

	std::lock_guard<std::mutex> lock(splitter_mutex_);
	int windowWidth = 0;
	int windowHeight = 0;
	if (window_)
	{
		SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
		std::cout << "[Splitter] Window size: " << windowWidth << "x" << windowHeight << std::endl;
	}
	else
	{
		std::cout << "[Splitter] WARNING: window_ is null" << std::endl;
	}

	// Initialize last_window_* on first call only
	if (last_window_width_ <= 0 && last_window_height_ <= 0 && windowWidth > 0 && windowHeight > 0)
	{
		last_window_width_ = windowWidth;
		last_window_height_ = windowHeight;
		std::cout << "[Splitter] First sync - initialized reference window size: " 
		          << last_window_width_ << "x" << last_window_height_ << std::endl;
	}

	bool hasGeometryChanged = hasSourceGeometryChanged(frameDataMap);
	bool shouldRefresh = shouldRefreshFromSource(frameDataMap);
	
	std::cout << "[Splitter] hasSourceGeometryChanged: " << (hasGeometryChanged ? "true" : "false") << std::endl;
	std::cout << "[Splitter] shouldRefreshFromSource: " << (shouldRefresh ? "true" : "false") << std::endl;

	bool windowSizeChanged = (windowWidth > 0 && windowHeight > 0 && 
	                          (windowWidth != last_window_width_ || windowHeight != last_window_height_));

	if (layout_ready_ && !hasGeometryChanged && !shouldRefresh && !windowSizeChanged)
	{
		std::cout << "[Splitter] No changes detected, skipping sync" << std::endl;
		std::cout << "---------------- [Splitter] syncFrameDatas END ----------------\n" << std::endl;
		return;
	}

	if (hasGeometryChanged || shouldRefresh)
	{
		std::cout << "[Splitter] Calling rebuildFromSource with current window size: " << windowWidth << "x" << windowHeight << std::endl;
		source_frame_data_map_ = frameDataMap;
		rebuildFromSource(frameDataMap, windowWidth, windowHeight);
		std::cout << "[Splitter] rebuildFromSource completed" << std::endl;
		
		// After rebuild, always check if we need to scale to current window size
		// Scale if: window size is valid AND (it changed OR panels have different window size)
		if (windowWidth > 0 && windowHeight > 0)
		{
			bool needsScaling = (windowWidth != last_window_width_ || windowHeight != last_window_height_);
			std::cout << "[Splitter] Checking scaling need: windowWidth=" << windowWidth << " vs last=" << last_window_width_ 
			          << ", windowHeight=" << windowHeight << " vs last=" << last_window_height_ 
			          << " -> needsScaling=" << (needsScaling ? "true" : "false") << std::endl;
			
			// Also check if any panel has different window dimensions (from CEF frame data)
			if (!needsScaling && !panel_state_map_.empty())
			{
				for (const auto& pair : panel_state_map_)
				{
					if (pair.second.frame.windowWidth != windowWidth || pair.second.frame.windowHeight != windowHeight)
					{
						needsScaling = true;
						std::cout << "[Splitter] Panel " << pair.first << " has mismatched window size: "
						          << pair.second.frame.windowWidth << "x" << pair.second.frame.windowHeight
						          << " (expected " << windowWidth << "x" << windowHeight << ")" << std::endl;
						break;
					}
				}
			}
			
			if (needsScaling)
			{
				std::cout << "[Splitter] Window size mismatch detected, calling scaleLayoutToWindow..." << std::endl;
				scaleLayoutToWindow(windowWidth, windowHeight);
				refreshDerivedData();
				std::cout << "[Splitter] Window scaling completed" << std::endl;
			}
		}
	}
	else if (windowWidth > 0 && windowHeight > 0 && (windowWidth != last_window_width_ || windowHeight != last_window_height_))
	{
		std::cout << "[Splitter] Window size changed, calling scaleLayoutToWindow..." << std::endl;
		scaleLayoutToWindow(windowWidth, windowHeight);
		refreshDerivedData();
		std::cout << "[Splitter] Window scaling completed" << std::endl;
	}
	else
	{
		std::cout << "[Splitter] No major changes, calling refreshDerivedData..." << std::endl;
		refreshDerivedData();
		std::cout << "[Splitter] refreshDerivedData completed" << std::endl;
	}

	std::cout << "[Splitter] Final state - layout_ready_: " << (layout_ready_ ? "true" : "false") << std::endl;
	std::cout << "[Splitter] Final state - panel_state_map_ size: " << panel_state_map_.size() << std::endl;
	
	for (const auto& pair : panel_state_map_)
	{
		std::cout << "[Splitter] Panel state: " << pair.first 
				  << " - X:" << pair.second.frame.relativeX 
				  << " Y:" << pair.second.frame.relativeY 
				  << " W:" << pair.second.frame.width 
				  << " H:" << pair.second.frame.height << std::endl;
	}

	std::cout << "---------------- [Splitter] syncFrameDatas END ----------------\n" << std::endl;
}

bool Splitter::handleEvent(const SDL_Event& event)
{
	if (!initialized_)
	{
		return false;
	}

	int mouseX = 0;
	int mouseY = 0;
	resolveMousePosition(event, mouseX, mouseY);

	std::lock_guard<std::mutex> lock(splitter_mutex_);
	if (!layout_ready_)
	{
		return false;
	}

	switch (event.type)
	{
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
		{
			if (window_)
			{
				int windowWidth = 0;
				int windowHeight = 0;
				SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
				if (windowWidth > 0 && windowHeight > 0 && (windowWidth != last_window_width_ || windowHeight != last_window_height_))
				{
					scaleLayoutToWindow(windowWidth, windowHeight);
					refreshDerivedData();
				}
			}

			return false;
		}

		case SDL_EVENT_MOUSE_MOTION:
		{
			if (dragging_ && active_splitter_index_ >= 0 && active_splitter_index_ < static_cast<int>(splitters_.size()))
			{
				const SplitterGeometry activeSplitter = splitters_[active_splitter_index_];
				int currentAnchor = activeSplitter.axis == Axis::Vertical ? mouseX : mouseY;
				int delta = currentAnchor - drag_anchor_;
				if (delta != 0)
				{
					int appliedDelta = applyDelta(activeSplitter, delta);
					if (appliedDelta != 0)
					{
						drag_anchor_ += appliedDelta;
						refreshDerivedData();
					}
				}
				hovered_splitter_index_ = active_splitter_index_;
				return true;
			}

			hovered_splitter_index_ = pickSplitterIndex(mouseX, mouseY);
			return hovered_splitter_index_ != -1;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		{
			if (event.button.button != SDL_BUTTON_LEFT)
			{
				return false;
			}

			const int splitterIndex = pickSplitterIndex(mouseX, mouseY);
			if (splitterIndex == -1)
			{
				return false;
			}

			beginDrag(splitterIndex, mouseX, mouseY);
			return true;
		}

		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			if (event.button.button != SDL_BUTTON_LEFT)
			{
				return false;
			}

			if (!dragging_)
			{
				return false;
			}

			endDrag();
			hovered_splitter_index_ = pickSplitterIndex(mouseX, mouseY);
			return true;
		}

		default:
		{
			return false;
		}
	}
}

bool Splitter::hasSourceGeometryChanged(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const
{
	if (source_frame_data_map_.size() != frameDataMap.size())
	{
		std::cout << "[Splitter::hasSourceGeometryChanged] Size changed: " 
		          << source_frame_data_map_.size() << " -> " << frameDataMap.size() << std::endl;
		return true;
	}

	for (const auto& pair : frameDataMap)
	{
		auto it = source_frame_data_map_.find(pair.first);
		if (it == source_frame_data_map_.end())
		{
			std::cout << "[Splitter::hasSourceGeometryChanged] New panel detected: " << pair.first << std::endl;
			return true;
		}

		const SIMILI::Frontend::IFrameScreenData& current = pair.second;
		const SIMILI::Frontend::IFrameScreenData& previous = it->second;
		if (current.relativeX != previous.relativeX || current.relativeY != previous.relativeY || 
		    current.width != previous.width || current.height != previous.height)
		{
			std::cout << "[Splitter::hasSourceGeometryChanged] Panel " << pair.first << " geometry changed:" << std::endl;
			std::cout << "  Previous: (" << previous.relativeX << "," << previous.relativeY 
			          << " " << previous.width << "x" << previous.height << ")" << std::endl;
			std::cout << "  Current:  (" << current.relativeX << "," << current.relativeY 
			          << " " << current.width << "x" << current.height << ")" << std::endl;
			return true;
		}
	}

	std::cout << "[Splitter::hasSourceGeometryChanged] No geometry changes detected" << std::endl;
	return false;
}

void Splitter::draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight)
{
	if (!initialized_ || !vk_context_ || !shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		return;
	}

	std::vector<SplitterGeometry> splitters;
	int hoveredSplitterIndex = -1;
	int activeSplitterIndex = -1;
	bool dragging = false;

	{
		std::lock_guard<std::mutex> lock(splitter_mutex_);
		if (!layout_ready_ || splitters_.empty())
		{
			return;
		}

		splitters = splitters_;
		hoveredSplitterIndex = hovered_splitter_index_;
		activeSplitterIndex = active_splitter_index_;
		dragging = dragging_;
	}

	if (drawableWidth <= 0 || drawableHeight <= 0)
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
	
	// Bind descriptor set (required even if shader doesn't use it - driver validation)
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->layout, 0, 1, &vk_descriptor_set_, 0, nullptr);

	for (std::size_t index = 0; index < splitters.size(); ++index)
	{
		const bool highlighted = static_cast<int>(index) == hoveredSplitterIndex || (dragging && static_cast<int>(index) == activeSplitterIndex);
		const glm::vec4 color = highlighted ? glm::vec4(0.12f, 0.78f, 0.24f, 1.0f) : glm::vec4(0.22f, 0.24f, 0.26f, 1.0f);
		drawGeometry(commandBuffer, splitters[index], drawableWidth, drawableHeight, color.r, color.g, color.b, color.a);
	}
}

bool Splitter::getViewportFrameData(SIMILI::Frontend::IFrameScreenData& outData) const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	auto it = panel_state_map_.find("viewport_panel");
	if (it == panel_state_map_.end())
	{
		return false;
	}

	outData = it->second.frame;
	return true;
}

std::map<std::string, SIMILI::Frontend::IFrameScreenData> Splitter::getUIPanelFrameDatas() const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> frameDataMap;
	
	static int call_count = 0;
	bool should_log = (call_count++ % 120 == 0);

	for (const auto& pair : panel_state_map_)
	{
		if (pair.second.frame.width <= 0 || pair.second.frame.height <= 0)
		{
			if (should_log)
			{
				std::cout << "[Splitter::getUIPanelFrameDatas] EXCLUDING " << pair.first 
				          << " - invalid dimensions: " << pair.second.frame.width 
				          << "x" << pair.second.frame.height << std::endl;
			}
			continue;
		}

		frameDataMap[pair.first] = pair.second.frame;
		
		if (should_log)
		{
			std::cout << "[Splitter::getUIPanelFrameDatas] INCLUDING " << pair.first 
			          << " at (" << pair.second.frame.relativeX << "," << pair.second.frame.relativeY 
			          << ") size " << pair.second.frame.width << "x" << pair.second.frame.height << std::endl;
		}
	}

	return frameDataMap;
}

std::map<std::string, SIMILI::Frontend::IFrameScreenData> Splitter::getAllFrameDatas() const
{
	std::lock_guard<std::mutex> lock(splitter_mutex_);
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> frameDataMap;

	for (const auto& pair : panel_state_map_)
	{
		frameDataMap[pair.first] = pair.second.frame;
	}

	return frameDataMap;
}

bool Splitter::createGraphicsResources()
{
	if (!vk_context_ || !vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
	{
		std::cerr << "[Splitter::createGraphicsResources] Missing VulkanPipeline or VKContext" << std::endl;
		return false;
	}

	if (vk_render_pass_ == VK_NULL_HANDLE)
	{
		std::cerr << "[Splitter::createGraphicsResources] RenderPass is VK_NULL_HANDLE" << std::endl;
		return false;
	}

	VkDevice device = vk_context_->getDevice();

	// GLSL Vertex Shader - Runtime compiled
	const std::string vertexShaderGLSL = R"(
	#version 450

	layout(location = 0) in vec2 inPosition;

	void main() {
		gl_Position = vec4(inPosition, 0.0, 1.0);
	}
	)";

	// GLSL Fragment Shader - Runtime compiled  
	const std::string fragmentShaderGLSL = R"(
	#version 450

	layout(location = 0) out vec4 outColor;

	void main() {
		outColor = vec4(0.22, 0.24, 0.26, 1.0);
	}
	)";

	// Compile vertex shader
	std::cout << "[Splitter] Compiling vertex shader..." << std::endl;
	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[Splitter] Failed to compile vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Compile fragment shader
	std::cout << "[Splitter] Compiling fragment shader..." << std::endl;
	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[Splitter] Failed to compile fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Convert SPIR-V to byte vectors
	std::vector<char> vertShaderBytes = GLSLCompiler::spirvToBytes(vertexSPIRV);
	std::vector<char> fragShaderBytes = GLSLCompiler::spirvToBytes(fragmentSPIRV);

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(float) * 12;  
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &vk_vertex_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[Splitter::createGraphicsResources] Failed to create vertex buffer" << std::endl;
		return false;
	}

	// Allocate vertex buffer memory
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
		std::cerr << "[Splitter::createGraphicsResources] Failed to allocate vertex buffer memory" << std::endl;
		return false;
	}

	vkBindBufferMemory(device, vk_vertex_buffer_, vk_vertex_buffer_memory_, 0);

	// Configure vertex attributes - SIMPLIFIED: only vec2 position (no texcoord)
	VkVertexInputAttributeDescription attr0{};
	attr0.binding = 0;
	attr0.location = 0;
	attr0.format = VK_FORMAT_R32G32_SFLOAT;
	attr0.offset = 0;

	std::vector<VkVertexInputAttributeDescription> vertexAttributes = { attr0 };

	// NVIDIA driver bug: Use SAME descriptor type as CEF_Drawer (texture sampler)
	// Even though shaders don't use it - driver validation requires exact match
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
		std::cerr << "[Splitter::createGraphicsResources] Failed to create descriptor set layout" << std::endl;
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_memory_ = VK_NULL_HANDLE;
		vk_vertex_buffer_ = VK_NULL_HANDLE;
		return false;
	}
	std::cout << "[Splitter::createGraphicsResources] Created descriptor set layout (texture sampler): " << vk_descriptor_set_layout_ << std::endl;

	// Create dummy 1x1 white texture for descriptor binding
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
		std::cerr << "[Splitter] Failed to create dummy texture image" << std::endl;
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
		std::cerr << "[Splitter] Failed to allocate dummy texture memory" << std::endl;
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	vkBindImageMemory(device, dummy_texture_image_, dummy_texture_memory_, 0);

	// Create image view
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
		std::cerr << "[Splitter] Failed to create dummy image view" << std::endl;
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	// Create sampler
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
		std::cerr << "[Splitter] Failed to create dummy sampler" << std::endl;
		vkDestroyImageView(device, dummy_texture_view_, nullptr);
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	std::cout << "[Splitter] Dummy texture created (1x1 for descriptor binding)" << std::endl;

	// Create descriptor pool
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
		std::cerr << "[Splitter] Failed to create descriptor pool" << std::endl;
		vkDestroySampler(device, dummy_texture_sampler_, nullptr);
		vkDestroyImageView(device, dummy_texture_view_, nullptr);
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		return false;
	}

	std::cout << "[Splitter] Descriptor pool created: " << vk_descriptor_pool_ << std::endl;

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfoDesc{};
	allocInfoDesc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfoDesc.descriptorPool = vk_descriptor_pool_;
	allocInfoDesc.descriptorSetCount = 1;
	allocInfoDesc.pSetLayouts = &vk_descriptor_set_layout_;

	if (vkAllocateDescriptorSets(device, &allocInfoDesc, &vk_descriptor_set_) != VK_SUCCESS)
	{
		std::cerr << "[Splitter] Failed to allocate descriptor set" << std::endl;
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

	std::cout << "[Splitter] Descriptor set allocated: " << vk_descriptor_set_ << std::endl;

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
	std::cout << "[Splitter] Descriptor set updated with dummy texture" << std::endl;

	// Build pipeline configuration
	VulkanPipeline::PipelineConfig config;
	config.name = "splitter_lines";
	config.vertexShaderCode = vertShaderBytes;
	config.fragmentShaderCode = fragShaderBytes;
	config.renderPass = vk_render_pass_;
	config.descriptorSetLayout = vk_descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 2 * sizeof(float);  // Changed from 4 to 2 (vec2 only)
	config.vertexAttributes = vertexAttributes;

	// Get or create pipeline from factory
	shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);
	
	if (!shared_pipeline_)
	{
		vkFreeMemory(device, vk_vertex_buffer_memory_, nullptr);
		vkDestroyBuffer(device, vk_vertex_buffer_, nullptr);
		vk_vertex_buffer_memory_ = VK_NULL_HANDLE;
		vk_vertex_buffer_ = VK_NULL_HANDLE;
		std::cerr << "[Splitter::createGraphicsResources] Failed to get/create pipeline" << std::endl;
		return false;
	}

	std::cout << "[Splitter::createGraphicsResources] Pipeline acquired successfully (handle=" 
	          << shared_pipeline_->pipeline << ")" << std::endl;

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

	// Release shared pipeline - VulkanPipeline will destroy when no longer referenced
	shared_pipeline_.reset();

	// Destroy descriptor resources
	if (vk_descriptor_pool_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, vk_descriptor_pool_, nullptr);
		vk_descriptor_pool_ = VK_NULL_HANDLE;
		vk_descriptor_set_ = VK_NULL_HANDLE;
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

	if (dummy_texture_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, dummy_texture_memory_, nullptr);
		dummy_texture_memory_ = VK_NULL_HANDLE;
	}

	if (dummy_texture_image_ != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, dummy_texture_image_, nullptr);
		dummy_texture_image_ = VK_NULL_HANDLE;
	}

	if (vk_descriptor_set_layout_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device, vk_descriptor_set_layout_, nullptr);
		vk_descriptor_set_layout_ = VK_NULL_HANDLE;
	}

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
}

bool Splitter::shouldRefreshFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const
{
	if (!layout_ready_)
	{
		std::cout << "[Splitter::shouldRefreshFromSource] Layout not ready - returning true" << std::endl;
		return true;
	}

	if (panel_state_map_.size() != frameDataMap.size())
	{
		std::cout << "[Splitter::shouldRefreshFromSource] Panel count changed: " 
		          << panel_state_map_.size() << " -> " << frameDataMap.size() << " - returning true" << std::endl;
		return true;
	}

	for (const auto& pair : frameDataMap)
	{
		if (panel_state_map_.find(pair.first) == panel_state_map_.end())
		{
			std::cout << "[Splitter::shouldRefreshFromSource] New panel detected: " << pair.first << " - returning true" << std::endl;
			return true;
		}
	}

	std::cout << "[Splitter::shouldRefreshFromSource] No refresh needed - returning false" << std::endl;
	return false;
}

void Splitter::rebuildFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap, int currentWindowWidth, int currentWindowHeight)
{
	panel_map_builder_.rebuildFromSource(
		frameDataMap,
		currentWindowWidth,
		currentWindowHeight,
		panel_state_map_,
		last_window_width_,
		last_window_height_
	);
	
	layout_ready_ = !panel_state_map_.empty();
	refreshDerivedData();
}

void Splitter::scaleLayoutToWindow(int newWindowWidth, int newWindowHeight)
{
	panel_map_builder_.scaleLayoutToWindow(
		newWindowWidth,
		newWindowHeight,
		panel_state_map_,
		last_window_width_,
		last_window_height_,
		window_
	);
}

void Splitter::refreshDerivedData()
{
	panel_map_builder_.updateClientCoordinates(panel_state_map_);
	rebuildSplitters();
	layout_ready_ = !panel_state_map_.empty();
	if (splitters_.empty())
	{
		hovered_splitter_index_ = -1;
		active_splitter_index_ = dragging_ ? active_splitter_index_ : -1;
	}
}

void Splitter::rebuildSplitters()
{
	splitters_.clear();

	auto getPanel = [this](const std::string& panelName) -> const SIMILI::Frontend::PanelState*
	{
		auto it = panel_state_map_.find(panelName);
		if (it == panel_state_map_.end())
		{
			return nullptr;
		}

		return &it->second;
	};

	const SIMILI::Frontend::PanelState* hierarchyPanel = getPanel("hierarchy_panel");
	const SIMILI::Frontend::PanelState* viewportPanel = getPanel("viewport_panel");
	const SIMILI::Frontend::PanelState* objectInspectorPanel = getPanel("object_inspector_panel");
	const SIMILI::Frontend::PanelState* historyPanel = getPanel("history_panel");
	const SIMILI::Frontend::PanelState* projectViewerPanel = getPanel("project_viewer_panel");

	int topRowTop = 0;
	int topRowBottom = 0;
	int topRowLeft = 0;
	int topRowRight = 0;
	bool hasTopRowBounds = false;

	for (const auto* panel : {hierarchyPanel, viewportPanel, objectInspectorPanel, historyPanel})
	{
		if (!panel)
		{
			continue;
		}

		const int left = panel->frame.relativeX;
		const int top = panel->frame.relativeY;
		const int right = panel->frame.relativeX + panel->frame.width;
		const int bottom = panel->frame.relativeY + panel->frame.height;

		if (!hasTopRowBounds)
		{
			topRowLeft = left;
			topRowTop = top;
			topRowRight = right;
			topRowBottom = bottom;
			hasTopRowBounds = true;
		}
		else
		{
			topRowLeft = std::min(topRowLeft, left);
			topRowTop = std::min(topRowTop, top);
			topRowRight = std::max(topRowRight, right);
			topRowBottom = std::max(topRowBottom, bottom);
		}
	}

	auto addVerticalSplitter = [this, topRowTop, topRowBottom](Type type, const SIMILI::Frontend::PanelState* leftPanel, const SIMILI::Frontend::PanelState* rightPanel)
	{
		if (!leftPanel || !rightPanel)
		{
			return;
		}

		const int boundary = leftPanel->frame.relativeX + leftPanel->frame.width;
		const int gap = rightPanel->frame.relativeX - boundary;
		const int thickness = gap > 0 ? gap : kDefaultSplitterThickness;
		const int x = gap > 0 ? boundary : boundary - thickness / 2;
		const int height = std::max(0, topRowBottom - topRowTop);
		if (height <= 0)
		{
			return;
		}

		splitters_.push_back({type, Axis::Vertical, x, topRowTop, thickness, height});
	};

	auto addHorizontalSplitter = [this](Type type, const SIMILI::Frontend::PanelState* topPanel, const SIMILI::Frontend::PanelState* bottomPanel)
	{
		if (!topPanel || !bottomPanel)
		{
			return;
		}

		const int boundary = topPanel->frame.relativeY + topPanel->frame.height;
		const int gap = bottomPanel->frame.relativeY - boundary;
		const int thickness = gap > 0 ? gap : kDefaultSplitterThickness;
		const int y = gap > 0 ? boundary : boundary - thickness / 2;
		const int left = std::min(topPanel->frame.relativeX, bottomPanel->frame.relativeX);
		const int right = std::max(topPanel->frame.relativeX + topPanel->frame.width, bottomPanel->frame.relativeX + bottomPanel->frame.width);
		const int width = std::max(0, right - left);
		if (width <= 0)
		{
			return;
		}

		splitters_.push_back({type, Axis::Horizontal, left, y, width, thickness});
	};

	if (hasTopRowBounds)
	{
		addVerticalSplitter(Type::HierarchyViewport, hierarchyPanel, viewportPanel);
		addVerticalSplitter(Type::ViewportInspector, viewportPanel, objectInspectorPanel);
	}

	addHorizontalSplitter(Type::InspectorHistory, objectInspectorPanel, historyPanel);

	if (hasTopRowBounds && projectViewerPanel)
	{
		const int boundary = topRowBottom;
		const int gap = projectViewerPanel->frame.relativeY - boundary;
		const int thickness = gap > 0 ? gap : kDefaultSplitterThickness;
		const int y = gap > 0 ? boundary : boundary - thickness / 2;
		const int left = std::min(topRowLeft, projectViewerPanel->frame.relativeX);
		const int right = std::max(topRowRight, projectViewerPanel->frame.relativeX + projectViewerPanel->frame.width);
		const int width = std::max(0, right - left);
		if (width > 0)
		{
			splitters_.push_back({Type::TopProjectViewer, Axis::Horizontal, left, y, width, thickness});
		}
	}
}

bool Splitter::resolveMousePosition(const SDL_Event& event, int& x, int& y) const
{
	switch (event.type)
	{
		case SDL_EVENT_MOUSE_MOTION:
		{
			x = static_cast<int>(std::lround(event.motion.x));
			y = static_cast<int>(std::lround(event.motion.y));
			return true;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			x = static_cast<int>(std::lround(event.button.x));
			y = static_cast<int>(std::lround(event.button.y));
			return true;
		}

		default:
		{
			float mouseX = 0.0f;
			float mouseY = 0.0f;
			SDL_GetMouseState(&mouseX, &mouseY);
			x = static_cast<int>(std::lround(mouseX));
			y = static_cast<int>(std::lround(mouseY));
			return false;
		}
	}
}

int Splitter::pickSplitterIndex(int x, int y) const
{
	for (std::size_t index = 0; index < splitters_.size(); ++index)
	{
		const SplitterGeometry& splitter = splitters_[index];
		const bool insideX = x >= splitter.x && x <= splitter.x + splitter.width;
		const bool insideY = y >= splitter.y && y <= splitter.y + splitter.height;
		if (insideX && insideY)
		{
			return static_cast<int>(index);
		}
	}

	return -1;
}

void Splitter::beginDrag(int splitterIndex, int x, int y)
{
	if (splitterIndex < 0 || splitterIndex >= static_cast<int>(splitters_.size()))
	{
		return;
	}

	dragging_ = true;
	active_splitter_index_ = splitterIndex;
	hovered_splitter_index_ = splitterIndex;
	drag_anchor_ = splitters_[splitterIndex].axis == Axis::Vertical ? x : y;
}

void Splitter::endDrag()
{
	dragging_ = false;
	active_splitter_index_ = -1;
	drag_anchor_ = 0;
}


int Splitter::applyDelta(const SplitterGeometry& splitter, int delta)
{
	switch (splitter.type)
	{
		case Type::HierarchyViewport:
		{
			return applyVerticalDelta("hierarchy_panel", "viewport_panel", delta, {});
		}

		case Type::ViewportInspector:
		{
			return applyVerticalDelta("viewport_panel", "object_inspector_panel", delta, {"history_panel"});
		}

		case Type::InspectorHistory:
		{
			return applyHorizontalDelta("object_inspector_panel", "history_panel", delta);
		}

		case Type::TopProjectViewer:
		{
			return applyTopRowDelta(delta);
		}
	}

	return 0;
}

int Splitter::applyVerticalDelta(const std::string& leftPanelName, const std::string& rightPanelName, int delta, const std::vector<std::string>& linkedRightPanels)
{
	auto leftIt = panel_state_map_.find(leftPanelName);
	auto rightIt = panel_state_map_.find(rightPanelName);
	if (leftIt == panel_state_map_.end() || rightIt == panel_state_map_.end())
	{
		return 0;
	}

	int maxPositiveDelta = rightIt->second.frame.width - kMinPanelWidth;
	for (const auto& panelName : linkedRightPanels)
	{
		auto linkedIt = panel_state_map_.find(panelName);
		if (linkedIt != panel_state_map_.end())
		{
			maxPositiveDelta = std::min(maxPositiveDelta, linkedIt->second.frame.width - kMinPanelWidth);
		}
	}

	const int maxNegativeDelta = leftIt->second.frame.width - kMinPanelWidth;
	const int clampedDelta = std::clamp(delta, -maxNegativeDelta, maxPositiveDelta);
	if (clampedDelta == 0)
	{
		return 0;
	}

	leftIt->second.frame.width += clampedDelta;
	rightIt->second.frame.relativeX += clampedDelta;
	rightIt->second.frame.width -= clampedDelta;

	for (const auto& panelName : linkedRightPanels)
	{
		auto linkedIt = panel_state_map_.find(panelName);
		if (linkedIt != panel_state_map_.end())
		{
			linkedIt->second.frame.relativeX += clampedDelta;
			linkedIt->second.frame.width -= clampedDelta;
		}
	}

	return clampedDelta;
}

int Splitter::applyHorizontalDelta(const std::string& topPanelName, const std::string& bottomPanelName, int delta)
{
	auto topIt = panel_state_map_.find(topPanelName);
	auto bottomIt = panel_state_map_.find(bottomPanelName);
	if (topIt == panel_state_map_.end() || bottomIt == panel_state_map_.end())
	{
		return 0;
	}

	const int maxNegativeDelta = topIt->second.frame.height - kMinPanelHeight;
	const int maxPositiveDelta = bottomIt->second.frame.height - kMinPanelHeight;
	const int clampedDelta = std::clamp(delta, -maxNegativeDelta, maxPositiveDelta);
	if (clampedDelta == 0)
	{
		return 0;
	}

	topIt->second.frame.height += clampedDelta;
	bottomIt->second.frame.relativeY += clampedDelta;
	bottomIt->second.frame.height -= clampedDelta;
	return clampedDelta;
}

int Splitter::applyTopRowDelta(int delta)
{
	auto hierarchyIt = panel_state_map_.find("hierarchy_panel");
	auto viewportIt = panel_state_map_.find("viewport_panel");
	auto objectInspectorIt = panel_state_map_.find("object_inspector_panel");
	auto historyIt = panel_state_map_.find("history_panel");
	auto projectViewerIt = panel_state_map_.find("project_viewer_panel");

	if (hierarchyIt == panel_state_map_.end() || viewportIt == panel_state_map_.end() || objectInspectorIt == panel_state_map_.end() || historyIt == panel_state_map_.end() || projectViewerIt == panel_state_map_.end())
	{
		return 0;
	}

	const int rightGap = historyIt->second.frame.relativeY - (objectInspectorIt->second.frame.relativeY + objectInspectorIt->second.frame.height);
	const int rightTotalHeight = objectInspectorIt->second.frame.height + rightGap + historyIt->second.frame.height;
	const int minRightTotalHeight = kMinPanelHeight + rightGap + kMinPanelHeight;

	const int maxNegativeDelta = std::min({
		hierarchyIt->second.frame.height - kMinPanelHeight,
		viewportIt->second.frame.height - kMinPanelHeight,
		rightTotalHeight - minRightTotalHeight
	});
	const int maxPositiveDelta = projectViewerIt->second.frame.height - kMinPanelHeight;
	const int clampedDelta = std::clamp(delta, -maxNegativeDelta, maxPositiveDelta);
	if (clampedDelta == 0)
	{
		return 0;
	}

	hierarchyIt->second.frame.height += clampedDelta;
	viewportIt->second.frame.height += clampedDelta;

	const int currentRightContentHeight = objectInspectorIt->second.frame.height + historyIt->second.frame.height;
	const int newRightTotalHeight = rightTotalHeight + clampedDelta;
	const int newRightContentHeight = newRightTotalHeight - rightGap;
	float ratio = 0.5f;
	if (currentRightContentHeight > 0)
	{
		ratio = static_cast<float>(objectInspectorIt->second.frame.height) / static_cast<float>(currentRightContentHeight);
	}

	int newObjectInspectorHeight = static_cast<int>(std::lround(static_cast<float>(newRightContentHeight) * ratio));
	newObjectInspectorHeight = std::clamp(newObjectInspectorHeight, kMinPanelHeight, newRightContentHeight - kMinPanelHeight);
	const int newHistoryHeight = newRightContentHeight - newObjectInspectorHeight;

	objectInspectorIt->second.frame.height = newObjectInspectorHeight;
	historyIt->second.frame.relativeY = objectInspectorIt->second.frame.relativeY + objectInspectorIt->second.frame.height + rightGap;
	historyIt->second.frame.height = newHistoryHeight;

	projectViewerIt->second.frame.relativeY += clampedDelta;
	projectViewerIt->second.frame.height -= clampedDelta;

	return clampedDelta;
}

void Splitter::drawGeometry(VkCommandBuffer commandBuffer, const SplitterGeometry& splitter, int drawableWidth, int drawableHeight, float red, float green, float blue, float alpha)
{
	if (!window_ || !vk_context_ || !shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE || vk_vertex_buffer_ == VK_NULL_HANDLE)
	{
		return;
	}

	int logicalWidth = 0;
	int logicalHeight = 0;
	SDL_GetWindowSize(window_, &logicalWidth, &logicalHeight);
	if (logicalWidth <= 0 || logicalHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0)
	{
		return;
	}

	const float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(logicalWidth);
	const float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(logicalHeight);
	const int x = static_cast<int>(std::lround(static_cast<float>(splitter.x) * scaleX));
	const int y = static_cast<int>(std::lround(static_cast<float>(splitter.y) * scaleY));
	const int width = std::max(1, static_cast<int>(std::lround(static_cast<float>(splitter.width) * scaleX)));
	const int height = std::max(1, static_cast<int>(std::lround(static_cast<float>(splitter.height) * scaleY)));

	const float left = (static_cast<float>(x) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	const float right = (static_cast<float>(x + width) / static_cast<float>(drawableWidth)) * 2.0f - 1.0f;
	// Y: With positive viewport, NDC Y=-1 is top, Y=+1 is bottom
	const float top = (static_cast<float>(y) / static_cast<float>(drawableHeight)) * 2.0f - 1.0f;
	const float bottom = (static_cast<float>(y + height) / static_cast<float>(drawableHeight)) * 2.0f - 1.0f;

	// Simplified vertex format: only vec2 position (no texcoord)
	const float vertices[] = {
		left, top,
		left, bottom,
		right, bottom,
		left, top,
		right, bottom,
		right, top
	};

	// Update vertex buffer with new geometry
	VkDevice device = vk_context_->getDevice();
	void* data = nullptr;
	if (vkMapMemory(device, vk_vertex_buffer_memory_, 0, sizeof(vertices), 0, &data) == VK_SUCCESS)
	{
		std::memcpy(data, vertices, sizeof(vertices));
		vkUnmapMemory(device, vk_vertex_buffer_memory_);
	}

	// Bind vertex buffer
	VkBuffer vertexBuffers[] = {vk_vertex_buffer_};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	// Draw the splitter geometry
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
}

uint32_t Splitter::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	if (!vk_context_)
	{
		return 0;
	}

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