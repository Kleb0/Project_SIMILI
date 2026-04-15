#include "Enable_UI_Debug_Tools.hpp"
#include "Border_DebugRectangle.hpp"
#include "../App_Border.hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include "../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include <iostream>

Enable_UI_Debug_Tools::Enable_UI_Debug_Tools()
	: vk_context_(nullptr)
	, vk_render_pass_(VK_NULL_HANDLE)
	, vulkan_pipelines_(nullptr)
	, app_border_(nullptr)
	, border_debug_rectangle_(nullptr)
	, debug_enabled_(true)
	, initialized_(false)
{
	std::cout << "[Enable_UI_Debug_Tools] Constructor called" << std::endl;
}

Enable_UI_Debug_Tools::~Enable_UI_Debug_Tools()
{
	shutdown();
}

void Enable_UI_Debug_Tools::initialize(VKContext* vkContext, VkRenderPass renderPass, VulkanPipeline* pipelines, App_Border* appBorder)
{
	std::cout << "[Enable_UI_Debug_Tools] initialize() called" << std::endl;

	if (initialized_)
	{
		std::cout << "[Enable_UI_Debug_Tools] Already initialized" << std::endl;
		return;
	}

	vk_context_ = vkContext;
	vk_render_pass_ = renderPass;
	vulkan_pipelines_ = pipelines;
	app_border_ = appBorder;

	if (!border_debug_rectangle_)
	{
		border_debug_rectangle_ = new Border_DebugRectangle();
		border_debug_rectangle_->setVulkanPipelines(vulkan_pipelines_);
		
		if (border_debug_rectangle_->initialize(vk_context_, vk_render_pass_))
		{
			std::cout << "[Enable_UI_Debug_Tools] Border_DebugRectangle initialized successfully" << std::endl;
		}
		else
		{
			std::cerr << "[Enable_UI_Debug_Tools] Failed to initialize Border_DebugRectangle" << std::endl;
			delete border_debug_rectangle_;
			border_debug_rectangle_ = nullptr;
			return;
		}
	}

	initialized_ = true;
	std::cout << "[Enable_UI_Debug_Tools] Initialization complete" << std::endl;
}

void Enable_UI_Debug_Tools::shutdown()
{
	std::cout << "[Enable_UI_Debug_Tools] shutdown() called" << std::endl;

	if (border_debug_rectangle_)
	{
		delete border_debug_rectangle_;
		border_debug_rectangle_ = nullptr;
	}

	initialized_ = false;
}

void Enable_UI_Debug_Tools::activateDebugRender()
{
	if (!initialized_)
	{
		std::cout << "[Enable_UI_Debug_Tools] activateDebugRender() - Not initialized yet" << std::endl;
		return;
	}

	if (!border_debug_rectangle_ && vk_context_ && vk_render_pass_ != VK_NULL_HANDLE && vulkan_pipelines_)
	{
		std::cout << "[Enable_UI_Debug_Tools] activateDebugRender() - Creating Border_DebugRectangle..." << std::endl;
		border_debug_rectangle_ = new Border_DebugRectangle();
		border_debug_rectangle_->setVulkanPipelines(vulkan_pipelines_);
		
		if (border_debug_rectangle_->initialize(vk_context_, vk_render_pass_))
		{
			std::cout << "[Enable_UI_Debug_Tools] Border_DebugRectangle initialized in activateDebugRender()" << std::endl;
		}
		else
		{
			std::cerr << "[Enable_UI_Debug_Tools] Failed to initialize Border_DebugRectangle in activateDebugRender()" << std::endl;
			delete border_debug_rectangle_;
			border_debug_rectangle_ = nullptr;
		}
	}

	if (app_border_ && !app_border_->isDebugLineEnabled())
	{
		app_border_->enableDebugLine(true);
		std::cout << "[Enable_UI_Debug_Tools] Debug line enabled" << std::endl;
	}
}

void Enable_UI_Debug_Tools::drawDebugTools(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight)
{
	if (!initialized_ || !debug_enabled_)
	{
		return;
	}

	if (border_debug_rectangle_ && app_border_)
	{
		border_debug_rectangle_->draw(commandBuffer, drawableWidth, drawableHeight, app_border_);
	}
}

void Enable_UI_Debug_Tools::setDebugEnabled(bool enabled)
{
	debug_enabled_ = enabled;
	
	if (app_border_)
	{
		app_border_->enableDebugLine(enabled);
	}
}

bool Enable_UI_Debug_Tools::isDebugEnabled() const
{
	return debug_enabled_;
}
