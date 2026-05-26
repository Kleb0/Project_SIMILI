#include "VKScene.Hpp"
#include "VKcontext.hpp"
#include "../SceneObjectContainer/SceneObjectContainer.hpp"
#include "../GLSL_Compiler/GLSLCompiler.hpp"
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
	: graphicsPipeline(VK_NULL_HANDLE),
	  pipelineLayout(VK_NULL_HANDLE),
	  renderPass(VK_NULL_HANDLE),
	  gridVertexBuffer(VK_NULL_HANDLE),
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
		if (graphicsPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vkctx->getDevice(), graphicsPipeline, nullptr);
		}
		if (pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vkctx->getDevice(), pipelineLayout, nullptr);
		}
		if (renderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(vkctx->getDevice(), renderPass, nullptr);
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

	std::vector<glm::vec3> gridVertices;
	const int gridSize = 10;

	for (int i = -gridSize / 2; i <= gridSize / 2; ++i)
	{
		gridVertices.push_back(glm::vec3(i, 0, -gridSize / 2));
		gridVertices.push_back(glm::vec3(i, 0, gridSize / 2));
		
		gridVertices.push_back(glm::vec3(-gridSize / 2, 0, i));
		gridVertices.push_back(glm::vec3(gridSize / 2, 0, i));
	}

	gridVertexCount = static_cast<uint32_t>(gridVertices.size());

	std::cout << "[VKScene] Initialized with " << gridVertexCount << " grid vertices" << std::endl;
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
