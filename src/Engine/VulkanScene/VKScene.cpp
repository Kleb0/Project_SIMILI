#include "VKScene.Hpp"
#include "VKcontext.hpp"
#include "../SceneObjectContainer/SceneObjectContainer.hpp"
#include "../GLSL_Compiler/GLSLCompiler.hpp"
#include "../VulkanPipeline/VulkanPipeline.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Camera/Camera.hpp"

std::string VKScene::generateSceneID()
{
	static const char alphanumeric[] = 
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, sizeof(alphanumeric) - 2);
	
	std::stringstream ss;
	ss << "VKScene-";
	
	for (int i = 0; i < 16; ++i) {
		ss << alphanumeric[dis(gen)];
	}
	
	return ss.str();
}

VKScene::VKScene()
	: gridVertexBuffer(VK_NULL_HANDLE),
	  gridVertexBufferMemory(VK_NULL_HANDLE),
	  gridVertexCount(0),
	  objectContainer_(nullptr)
{
	sceneID = generateSceneID();
	std::cout << "[VKScene] Created with ID: " << sceneID << std::endl;
}

VKScene::~VKScene()
{
	if (vkctx && vkctx->getDevice() != VK_NULL_HANDLE) {
		if (gridVertexBuffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(vkctx->getDevice(), gridVertexBuffer, nullptr);
		}
		if (gridVertexBufferMemory != VK_NULL_HANDLE) {
			vkFreeMemory(vkctx->getDevice(), gridVertexBufferMemory, nullptr);
		}
		if (background_vertex_buffer_ != VK_NULL_HANDLE) {
			vkDestroyBuffer(vkctx->getDevice(), background_vertex_buffer_, nullptr);
		}
		if (background_vertex_memory_ != VK_NULL_HANDLE) {
			vkFreeMemory(vkctx->getDevice(), background_vertex_memory_, nullptr);
		}
	}
}

static inline void erasePtr(std::list<ThreeDObject*>& L, ThreeDObject* p)
{
	L.remove(p);
}

glm::mat4 VKScene::getViewMatrix() const
{
	if (!activeCamera) return glm::mat4(1.0f);
	return activeCamera->getViewMatrix();
}

glm::mat4 VKScene::getProjectionMatrix() const
{
	if (!activeCamera) return glm::mat4(1.0f);
	float aspect = 1.0f;
	if (vkctx && vkctx->getHeight() > 0)
		aspect = float(vkctx->getWidth()) / float(vkctx->getHeight());
	return activeCamera->getProjectionMatrix(aspect);
}

glm::mat4 VKScene::getViewProjMatrix() const
{
	return getProjectionMatrix() * getViewMatrix();
}

void VKScene::setActiveCamera(Camera* cam)
{
	if (!cam) {
		activeCamera = nullptr;
		return;
	}
	activeCamera = cam;
	cam->setVKScene(this);
}

void VKScene::setCameraToUse(Camera* cam)
{
	if (cam && objectContainer_)
	{
		setActiveCamera(cam);
		std::cout << "[VKScene] Camera attached to scene: " << cam->getName() << std::endl;
	}
}

void VKScene::setSceneObjectContainer(SceneObjectContainer* container)
{
	objectContainer_ = container;
	std::cout << "[VKScene] SceneObjectContainer set" << std::endl;
}

static inline void clearSelectionRecursive(ThreeDObject* o)
{
	if (!o) return;
	o->setSelected(false);
	for (auto* child : o->getChildren())
		clearSelectionRecursive(child);
}

static inline void collectAllChildrenRecursive(ThreeDObject* obj, std::vector<ThreeDObject*>& allChildren)
{
	if (!obj) return;
	
	for (auto* child : obj->getChildren())
	{
		allChildren.push_back(child);
		collectAllChildrenRecursive(child, allChildren);
	}
}

bool VKScene::containsObject(const ThreeDObject* obj) const
{
	if (objectContainer_) {
		return objectContainer_->containsObject(obj);
	}
	return std::find(objects.begin(), objects.end(), obj) != objects.end();
}

std::list<ThreeDObject*>& VKScene::getObjectsRef()
{
	if (objectContainer_) {
		return objectContainer_->getObjectsRef();
	}
	return objects;
}

void VKScene::pushInGraveyard(ThreeDObject* obj)
{
	if (!obj) return;

	clearSelectionRecursive(obj);
	
	std::vector<ThreeDObject*> allChildren;
	collectAllChildrenRecursive(obj, allChildren);
	
	std::list<ThreeDObject*>& objectsList = getObjectsRef();
	
	for (auto* child : allChildren)
	{
		erasePtr(objectsList, child);
		graveyard.push_back(child);
	}
	
	erasePtr(objectsList, obj);
	graveyard.push_back(obj);

	if (auto* mesh = dynamic_cast<Mesh*>(obj))
	{
		VKGraveyardEntry entry;
		entry.object = obj;
		entry.vertices = mesh->getVertices();
		entry.edges = mesh->getEdges();
		entry.faces = mesh->getFaces();
		meshGraveyard.push_back(entry);
	}
}

void VKScene::initialize()
{
	if (!vkctx) {
		std::cerr << "[VKScene] No VKContext set, cannot initialize" << std::endl;
		return;
	}

	std::cout << "[VKScene] Initialized with ID: " << sceneID << std::endl;
}

void VKScene::resize(int w, int h)
{
	if (vkctx) {
		vkctx->resize(w, h);
	}
}

void VKScene::render(int width, int height)
{
	if (!vkctx) {
		std::cerr << "[VKScene] No VKContext set, cannot render" << std::endl;
		return;
	}
	if (!activeCamera) {
		std::cerr << "[VKScene] No active camera, cannot render" << std::endl;
		return;
	}

	const float aspect = (height > 0) ? float(width) / float(height) : 1.0f;

	glm::mat4 view = activeCamera->getViewMatrix();
	glm::mat4 proj = activeCamera->getProjectionMatrix(aspect);
	glm::mat4 viewProj = proj * view;

	std::list<ThreeDObject*>& objectsList = getObjectsRef();
	for (auto* obj : objectsList) {
		obj->render(viewProj);
	}
}

void VKScene::addObject(ThreeDObject* object)
{
	if (!object) return;

	if (objectContainer_) {
		objectContainer_->addObject(object);
	} else {
		objects.push_back(object);
	}
	
	std::cout << "[VKScene] Adding object: " << object->getName() << std::endl;

	object->initialize();
}

bool VKScene::removeObject(ThreeDObject* object)
{
	if (!object) {
		return false;
	}

	if (!containsObject(object))
	{
		return false;
	}

	std::cout << "[VKScene] Removing object: " << object->getName() << " with ID: " << object->getID() << std::endl;

	if (object->getParent())
	{
		object->getParent()->removeChild(object);
	}

	pushInGraveyard(object);
	return true;
}

void VKScene::bindSceneViewToWorkSpaceDimensions(int logicalX, int logicalY, int logicalWidth, int logicalHeight,
                                                  int drawableWidth, int drawableHeight,
                                                  int logicalWindowWidth, int logicalWindowHeight)
{
	if (logicalWindowWidth <= 0 || logicalWindowHeight <= 0 || drawableWidth <= 0 || drawableHeight <= 0)
	{
		scene_viewport_bound_ = false;
		return;
	}

	float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(logicalWindowWidth);
	float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(logicalWindowHeight);

	int pixelX = static_cast<int>(static_cast<float>(logicalX) * scaleX);
	int pixelY_top = static_cast<int>(static_cast<float>(logicalY) * scaleY);
	int pixelW = static_cast<int>(static_cast<float>(logicalWidth) * scaleX);
	int pixelH = static_cast<int>(static_cast<float>(logicalHeight) * scaleY);

	scene_viewport_x_ = pixelX;
	scene_viewport_y_ = pixelY_top;
	scene_viewport_width_ = pixelW;
	scene_viewport_height_ = pixelH;

	workspace_logical_x_ = logicalX;
	workspace_logical_y_ = logicalY;
	workspace_logical_width_ = logicalWidth;
	workspace_logical_height_ = logicalHeight;

	if (scene_viewport_width_ <= 0 || scene_viewport_height_ <= 0)
	{
		scene_viewport_bound_ = false;
		return;
	}

	scene_viewport_bound_ = true;
}

void VKScene::renderRedScreenOnWorkSpaceDimensionsFirst(VkCommandBuffer commandBuffer, int mouseX, int mouseY)
{
	if (!scene_viewport_bound_ || commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	const bool isHovered =
		mouseX >= workspace_logical_x_ &&
		mouseX < workspace_logical_x_ + workspace_logical_width_ &&
		mouseY >= workspace_logical_y_ &&
		mouseY < workspace_logical_y_ + workspace_logical_height_;

	if (isHovered && !hover_active_)
	{
		static std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		hover_r_ = dist(rng);
		hover_g_ = dist(rng);
		hover_b_ = dist(rng);
		hover_active_ = true;
	}
	else if (!isHovered)
	{
		hover_active_ = false;
	}

	VkViewport viewport{};
	viewport.x = static_cast<float>(scene_viewport_x_);
	viewport.y = static_cast<float>(scene_viewport_y_);
	viewport.width = static_cast<float>(scene_viewport_width_);
	viewport.height = static_cast<float>(scene_viewport_height_);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { scene_viewport_x_, scene_viewport_y_ };
	scissor.extent = { static_cast<uint32_t>(scene_viewport_width_), static_cast<uint32_t>(scene_viewport_height_) };
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	VkClearAttachment clearAttachment{};
	clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearAttachment.colorAttachment = 0;
	clearAttachment.clearValue.color = hover_active_
		? VkClearColorValue{{ hover_r_, hover_g_, hover_b_, 1.0f }}
		: VkClearColorValue{{ 0.22f, 0.28f, 0.35f, 1.0f }};

	VkClearRect clearRect{};
	clearRect.rect = scissor;
	clearRect.baseArrayLayer = 0;
	clearRect.layerCount = 1;

	vkCmdClearAttachments(commandBuffer, 1, &clearAttachment, 1, &clearRect);
}

void VKScene::setVulkanPipelines(VulkanPipeline* pipelines, VkRenderPass renderPass)
{
	vulkan_pipelines_ = pipelines;
	scene_render_pass_ = renderPass;

	if (vulkan_pipelines_ && scene_render_pass_ != VK_NULL_HANDLE && vkctx)
	{
		if (!initializeScenePipelines())
		{
			std::cerr << "[VKScene] Failed to initialize scene pipelines" << std::endl;
		}
		else
		{
			std::cout << "[VKScene] Scene pipelines initialized successfully" << std::endl;
		}
	}
}

bool VKScene::createVulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags requiredProps, VkBuffer& outBuffer, VkDeviceMemory& outMemory)
{
	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = size;
	bufInfo.usage = usage;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(vkctx->getDevice(), &bufInfo, nullptr, &outBuffer) != VK_SUCCESS)
		return false;

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(vkctx->getDevice(), outBuffer, &memReq);

	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(vkctx->getPhysicalDevice(), &memProps);

	uint32_t memIdx = UINT32_MAX;
	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if ((memReq.memoryTypeBits & (1u << i)) &&
			(memProps.memoryTypes[i].propertyFlags & requiredProps) == requiredProps)
		{
			memIdx = i;
			break;
		}
	}

	if (memIdx == UINT32_MAX)
	{
		vkDestroyBuffer(vkctx->getDevice(), outBuffer, nullptr);
		outBuffer = VK_NULL_HANDLE;
		return false;
	}

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memIdx;

	if (vkAllocateMemory(vkctx->getDevice(), &allocInfo, nullptr, &outMemory) != VK_SUCCESS)
	{
		vkDestroyBuffer(vkctx->getDevice(), outBuffer, nullptr);
		outBuffer = VK_NULL_HANDLE;
		return false;
	}

	vkBindBufferMemory(vkctx->getDevice(), outBuffer, outMemory, 0);
	return true;
}

bool VKScene::createGridVertexBuffer()
{
	const int cellCount = 5;
	const float step = 1.0f;
	const float half = (cellCount / 2.0f) * step;

	std::vector<glm::vec3> vertices;

	for (int i = 0; i <= cellCount; i++)
	{
		float z = -half + i * step;
		vertices.push_back(glm::vec3(-half, 0.0f, z));
		vertices.push_back(glm::vec3( half, 0.0f, z));
	}

	for (int i = 0; i <= cellCount; i++)
	{
		float x = -half + i * step;
		vertices.push_back(glm::vec3(x, 0.0f, -half));
		vertices.push_back(glm::vec3(x, 0.0f,  half));
	}

	gridVertexCount = static_cast<uint32_t>(vertices.size());
	VkDeviceSize size = vertices.size() * sizeof(glm::vec3);

	if (!createVulkanBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		gridVertexBuffer, gridVertexBufferMemory))
	{
		return false;
	}

	void* data;
	vkMapMemory(vkctx->getDevice(), gridVertexBufferMemory, 0, size, 0, &data);
	memcpy(data, vertices.data(), static_cast<size_t>(size));
	vkUnmapMemory(vkctx->getDevice(), gridVertexBufferMemory);
	return true;
}

bool VKScene::createBackgroundVertexBuffer()
{
	const std::vector<glm::vec2> verts = {
		{-1.0f, -1.0f}, { 1.0f, -1.0f}, { 1.0f,  1.0f},
		{-1.0f, -1.0f}, { 1.0f,  1.0f}, {-1.0f,  1.0f}
	};

	VkDeviceSize size = verts.size() * sizeof(glm::vec2);

	if (!createVulkanBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		background_vertex_buffer_, background_vertex_memory_))
	{
		return false;
	}

	void* data;
	vkMapMemory(vkctx->getDevice(), background_vertex_memory_, 0, size, 0, &data);
	memcpy(data, verts.data(), static_cast<size_t>(size));
	vkUnmapMemory(vkctx->getDevice(), background_vertex_memory_);
	return true;
}

bool VKScene::initializeScenePipelines()
{
	if (!vulkan_pipelines_ || scene_render_pass_ == VK_NULL_HANDLE || !vkctx)
		return false;

	const std::string bgVertGLSL = R"(
	#version 450
	layout(location = 0) in vec2 aPosition;
	layout(location = 0) out float vNormalizedY;
	void main() {
		gl_Position = vec4(aPosition, 0.0, 1.0);
		vNormalizedY = (aPosition.y + 1.0) * 0.5;
	}
	)";

		const std::string bgFragGLSL = R"(
	#version 450
	layout(location = 0) in float vNormalizedY;
	layout(location = 0) out vec4 outColor;
	void main() {
		vec3 bottomColor = vec3(0.05, 0.05, 0.05);
		vec3 topColor    = vec3(0.30, 0.30, 0.35);
		outColor = vec4(mix(bottomColor, topColor, vNormalizedY), 1.0);
	}
	)";

	auto bgVertSpirv = GLSLCompiler::compileGLSL(bgVertGLSL, GLSLCompiler::ShaderType::Vertex);
	auto bgFragSpirv = GLSLCompiler::compileGLSL(bgFragGLSL, GLSLCompiler::ShaderType::Fragment);

	if (bgVertSpirv.empty() || bgFragSpirv.empty())
	{
		std::cerr << "[VKScene] Failed to compile background shaders: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	auto bgVertBytes = GLSLCompiler::spirvToBytes(bgVertSpirv);
	auto bgFragBytes = GLSLCompiler::spirvToBytes(bgFragSpirv);

	VkVertexInputAttributeDescription bgAttr{};
	bgAttr.binding = 0;
	bgAttr.location = 0;
	bgAttr.format = VK_FORMAT_R32G32_SFLOAT;
	bgAttr.offset = 0;

	VulkanPipeline::PipelineConfig bgConfig;
	bgConfig.name = "scene_background";
	bgConfig.vertexShaderCode = bgVertBytes;
	bgConfig.fragmentShaderCode = bgFragBytes;
	bgConfig.renderPass = scene_render_pass_;
	bgConfig.descriptorSetLayout = VK_NULL_HANDLE;
	bgConfig.enableBlending = false;
	bgConfig.topology             = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	bgConfig.vertexBindingStride  = static_cast<uint32_t>(sizeof(glm::vec2));
	bgConfig.vertexAttributes     = { bgAttr };

	auto bgPipeline = vulkan_pipelines_->getOrCreatePipeline(bgConfig);
	if (!bgPipeline || bgPipeline->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[VKScene] Failed to create background pipeline" << std::endl;
		return false;
	}

	background_pipeline_handle_ = bgPipeline->pipeline;
	background_pipeline_layout_ = bgPipeline->layout;

	const std::string gridVertGLSL = R"(
	#version 450
	layout(location = 0) in vec3 aPosition;
	layout(push_constant) uniform PushData {
		mat4 mvp;
	} push;
	void main() {
		gl_Position = push.mvp * vec4(aPosition, 1.0);
	}
	)";

		const std::string gridFragGLSL = R"(
	#version 450
	layout(location = 0) out vec4 outColor;
	void main() {
		outColor = vec4(0.65, 0.65, 0.70, 1.0);
	}
	)";

	auto gridVertSpirv = GLSLCompiler::compileGLSL(gridVertGLSL, GLSLCompiler::ShaderType::Vertex);
	auto gridFragSpirv = GLSLCompiler::compileGLSL(gridFragGLSL, GLSLCompiler::ShaderType::Fragment);

	if (gridVertSpirv.empty() || gridFragSpirv.empty())
	{
		std::cerr << "[VKScene] Failed to compile grid shaders: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	auto gridVertBytes = GLSLCompiler::spirvToBytes(gridVertSpirv);
	auto gridFragBytes = GLSLCompiler::spirvToBytes(gridFragSpirv);

	VkVertexInputAttributeDescription gridAttr{};
	gridAttr.binding = 0;
	gridAttr.location = 0;
	gridAttr.format = VK_FORMAT_R32G32B32_SFLOAT;
	gridAttr.offset = 0;

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushRange.offset = 0;
	pushRange.size = static_cast<uint32_t>(sizeof(glm::mat4));

	VulkanPipeline::PipelineConfig gridConfig;
	gridConfig.name  = "scene_grid";
	gridConfig.vertexShaderCode = gridVertBytes;
	gridConfig.fragmentShaderCode = gridFragBytes;
	gridConfig.renderPass = scene_render_pass_;
	gridConfig.descriptorSetLayout  = VK_NULL_HANDLE;
	gridConfig.enableBlending = false;
	gridConfig.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	gridConfig.vertexBindingStride  = static_cast<uint32_t>(sizeof(glm::vec3));
	gridConfig.vertexAttributes = { gridAttr };
	gridConfig.pushConstantRanges = { pushRange };

	auto gridPipeline = vulkan_pipelines_->getOrCreatePipeline(gridConfig);
	if (!gridPipeline || gridPipeline->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[VKScene] Failed to create grid pipeline" << std::endl;
		return false;
	}

	grid_pipeline_handle_ = gridPipeline->pipeline;
	grid_pipeline_layout_ = gridPipeline->layout;

	if (!createBackgroundVertexBuffer())
	{
		std::cerr << "[VKScene] Failed to create background vertex buffer" << std::endl;
		return false;
	}

	if (!createGridVertexBuffer())
	{
		std::cerr << "[VKScene] Failed to create grid vertex buffer" << std::endl;
		return false;
	}

	scene_pipeline_initialized_ = true;
	return true;
}

void VKScene::drawThreeDScene(VkCommandBuffer commandBuffer, int vpX, int vpY, int vpW, int vpH,
	const glm::mat4& view, const glm::mat4& proj)
{
	if (commandBuffer == VK_NULL_HANDLE || vpW <= 0 || vpH <= 0)
		return;

	if (!scene_pipeline_initialized_)
		return;

	VkViewport viewport{};
	viewport.x = static_cast<float>(vpX);
	viewport.y = static_cast<float>(vpY);
	viewport.width = static_cast<float>(vpW);
	viewport.height = static_cast<float>(vpH);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { vpX, vpY };
	scissor.extent = { static_cast<uint32_t>(vpW), static_cast<uint32_t>(vpH) };
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	if (background_pipeline_handle_ != VK_NULL_HANDLE && background_vertex_buffer_ != VK_NULL_HANDLE)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, background_pipeline_handle_);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &background_vertex_buffer_, &offset);
		vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	}

	if (grid_pipeline_handle_ != VK_NULL_HANDLE && gridVertexBuffer != VK_NULL_HANDLE)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, grid_pipeline_handle_);
		glm::mat4 mvp = proj * view;
		vkCmdPushConstants(commandBuffer, grid_pipeline_layout_,
			VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &gridVertexBuffer, &offset);
		vkCmdDraw(commandBuffer, gridVertexCount, 1, 0, 0);
	}
}
