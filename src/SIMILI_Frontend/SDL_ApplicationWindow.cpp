#include "SDL_ApplicationWindow.hpp"
#include "ui_handler.hpp"
#include "viewportLogic/ThreeDScreen/ThreeDScreen.hpp"
#include "viewportLogic/UIPanels/UIManager.hpp"
#include "viewportLogic/UIPanels/Splitter.hpp"
#include "CEFDrawing/CEF_Drawer.hpp"
#include "CEFDrawing/CEF_Resizer.hpp"
#include "App_Border.hpp"
#include "Frontend_Debug_Tools/Enable_UI_Debug_Tools.hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include <SDL3/SDL_vulkan.h>

SDL_ApplicationWindow::SDL_ApplicationWindow()
	: window_(nullptr)
	, is_maximized_(false)
	, last_x_(0)
	, last_y_(0)
	, last_width_(800)
	, last_height_(600)
	, dpi_scale_(1.0f)
	, window_state_(WindowRenderState::Init)
	, ui_handler_(nullptr)
	, threed_screen_(nullptr)
	, frame_datas_(nullptr)
	, ui_manager_(nullptr)
	, app_border_(nullptr)
	, debug_tools_(nullptr)
	, vk_context_(nullptr)
	, vulkan_pipelines_(nullptr)
	, vk_surface_(VK_NULL_HANDLE)
	, vk_swapchain_(VK_NULL_HANDLE)
	, vk_render_pass_(VK_NULL_HANDLE)
	, vk_command_pool_(VK_NULL_HANDLE)
	, current_image_index_(0)
	, current_frame_(0)
	, vk_image_available_semaphore_(VK_NULL_HANDLE)
	, vk_render_finished_semaphore_(VK_NULL_HANDLE)
	, vk_in_flight_fence_(VK_NULL_HANDLE)
	, swapchain_needs_recreation_(false)
{
}
SDL_ApplicationWindow::~SDL_ApplicationWindow()
{
	if (debug_tools_)
	{
		delete debug_tools_;
		debug_tools_ = nullptr;
	}
	if (app_border_)
	{
		delete app_border_;
		app_border_ = nullptr;
	}
	cleanupVulkan();
	destroy();
}

// ========= Lifecycle  ======== //


bool SDL_ApplicationWindow::create(const std::string& title, int width, int height, Uint32 flags)
{
	std::cout << "[SDL_ApplicationWindow] create() called with: " << title << " (" << width << "x" << height << ")" << std::endl;
	
	if (window_)
	{
		std::cerr << "[SDL_ApplicationWindow] Window already exists" << std::endl;
		return false;
	}
	
	std::cout << "[SDL_ApplicationWindow] Checking flags..." << std::endl;
	// Default flags if none specified - ensure Vulkan compatibility
	if (flags == 0)
	{
		flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	}
	
	// Ensure Vulkan flag is always present for this application
	if (!(flags & SDL_WINDOW_VULKAN))
	{
		flags |= SDL_WINDOW_VULKAN;
		std::cout << "[SDL_ApplicationWindow] Added SDL_WINDOW_VULKAN flag" << std::endl;
	}
	
	std::cout << "[SDL_ApplicationWindow] Final flags: 0x" << std::hex << flags << std::dec << std::endl;
	
	// Check SDL video subsystem state
	std::cout << "[SDL_ApplicationWindow] Checking SDL video subsystem..." << std::endl;
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		std::cerr << "[SDL_ApplicationWindow] SDL video subsystem not initialized!" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] SDL video subsystem is initialized" << std::endl;
	
	// Get current video driver info
	const char* driver = SDL_GetCurrentVideoDriver();
	std::cout << "[SDL_ApplicationWindow] Current video driver: " << (driver ? driver : "NULL") << std::endl;
	
	std::cout << "[SDL_ApplicationWindow] Checking Vulkan availability..." << std::endl;
	
	uint32_t extensionCount = 0;
	const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
	if (!extensions)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to get Vulkan extensions: " << SDL_GetError() << std::endl;
		return false;
	}
	
	std::cout << "[SDL_ApplicationWindow] Vulkan extensions available: " << extensionCount << std::endl;
	
	std::cout << "[SDL_ApplicationWindow] Final adjusted flags: 0x" << std::hex << flags << std::dec << std::endl;
	std::cout << "[SDL_ApplicationWindow] About to call SDL_CreateWindow..." << std::endl;
	window_ = SDL_CreateWindow(title.c_str(), width, height, flags);
	std::cout << "[SDL_ApplicationWindow] SDL_CreateWindow returned: " << (window_ ? "SUCCESS" : "FAILED") << std::endl;
	
	if (!window_)
	{
		const char* error = SDL_GetError();
		std::cerr << "[SDL_ApplicationWindow] Failed to create window: " << error << std::endl;
		std::cerr << "[SDL_ApplicationWindow] Requested: " << title << " (" << width << "x" << height << ")" << std::endl;
		std::cerr << "[SDL_ApplicationWindow] Flags: 0x" << std::hex << flags << std::dec << std::endl;
		
		// Try to get more information about available displays
		const SDL_DisplayID* displays = SDL_GetDisplays(nullptr);
		if (displays)
		{
			int numDisplays = 0;
			while (displays[numDisplays] != 0)
			{
				numDisplays++;
			}
			
			std::cerr << "[SDL_ApplicationWindow] Available displays: " << numDisplays << std::endl;
			for (int i = 0; i < numDisplays; i++)
			{
				const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displays[i]);
				if (mode)
				{
					std::cerr << "[SDL_ApplicationWindow] Display " << i << ": " << mode->w << "x" << mode->h 
					          << " @ " << mode->refresh_rate << "Hz" << std::endl;
				}
			}
			SDL_free((void*)displays);
		}
		
		return false;
	}
	
	last_width_ = width;
	last_height_ = height;
	
	std::cout << "[SDL_ApplicationWindow] Getting initial position..." << std::endl;
	// Get initial position
	SDL_GetWindowPosition(window_, &last_x_, &last_y_);
	std::cout << "[SDL_ApplicationWindow] Initial position: (" << last_x_ << ", " << last_y_ << ")" << std::endl;
	
	std::cout << "[SDL_ApplicationWindow] Updating DPI scale..." << std::endl;
	updateDpiScale();
	std::cout << "[SDL_ApplicationWindow] DPI scale: " << dpi_scale_ << std::endl;
	
	std::cout << "[SDL_ApplicationWindow] Updating maximized state..." << std::endl;
	updateMaximizedState();
	
	std::cout << "[SDL_ApplicationWindow] Window created: " << title 
	          << " (" << width << "x" << height << ") DPI scale: " << dpi_scale_ << std::endl;
	
	if (!app_border_)
	{
		app_border_ = new App_Border();
	}
	app_border_->setState(BorderState::Init);
	app_border_->updateDimensions(width, height);
	
	std::cout << "[SDL_ApplicationWindow] App_Border initialized with state: Init" << std::endl;
	
	if (!debug_tools_)
	{
		debug_tools_ = new Enable_UI_Debug_Tools();
	}
	
	return true;
}

void SDL_ApplicationWindow::destroy()
{
	// ui_manager_ is not owned by SDL_ApplicationWindow, so we don't delete it
	
	if (window_)
	{
		SDL_DestroyWindow(window_);
		window_ = nullptr;
		std::cout << "[SDL_ApplicationWindow] Window destroyed" << std::endl;
	}
}


// ======== Window Management ======== //

void SDL_ApplicationWindow::setPosition(int x, int y)
{
	if (window_)
	{
		SDL_SetWindowPosition(window_, x, y);
		last_x_ = x;
		last_y_ = y;
	}
}


void SDL_ApplicationWindow::setSize(int width, int height)
{
	if (window_)
	{
		SDL_SetWindowSize(window_, width, height);
		last_width_ = width;
		last_height_ = height;
	}
}

void SDL_ApplicationWindow::setTitle(const std::string& title)
{
	if (window_)
	{
		SDL_SetWindowTitle(window_, title.c_str());
	}
}

void SDL_ApplicationWindow::show()
{
	if (window_)
	{
		SDL_ShowWindow(window_);
	}
}

void SDL_ApplicationWindow::hide()
{
	if (window_)
	{
		SDL_HideWindow(window_);
	}
}

void SDL_ApplicationWindow::maximize()
{
	if (window_)
	{
		SDL_MaximizeWindow(window_);
		updateMaximizedState();
	}
}

void SDL_ApplicationWindow::restore()
{
	if (window_)
	{
		SDL_RestoreWindow(window_);
		updateMaximizedState();
	}
}

bool SDL_ApplicationWindow::isMaximized() const
{
	if (!window_)
	{
		return false;
	}
	
	Uint32 flags = SDL_GetWindowFlags(window_);
	bool maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
	
	return maximized;
}

// ======== Window Properties ======== //

void SDL_ApplicationWindow::getPosition(int& x, int& y) const
{
	if (window_)
	{
		SDL_GetWindowPosition(window_, &x, &y);
	}
	else
	{
		x = last_x_;
		y = last_y_;
	}
}

void SDL_ApplicationWindow::getSize(int& width, int& height) const
{
	if (window_)
	{
		SDL_GetWindowSize(window_, &width, &height);
	}
	else
	{
		width = last_width_;
		height = last_height_;
	}
}

SDL_Rect SDL_ApplicationWindow::getBounds() const
{
	SDL_Rect bounds;
	getPosition(bounds.x, bounds.y);
	getSize(bounds.w, bounds.h);
	return bounds;
}

void SDL_ApplicationWindow::getBorderOffsets(int& left, int& top, int& right, int& bottom) const
{
	left = top = right = bottom = 0;
	
	if (!window_ || !isMaximized())
		return;
	
	SDL_Rect borderSize;
	if (SDL_GetWindowBordersSize(window_, &borderSize.y, &borderSize.x, &borderSize.h, &borderSize.w) == 0)
	{
		left = borderSize.x;
		top = borderSize.y;
		right = borderSize.w;
		bottom = borderSize.h;
	}
}

float SDL_ApplicationWindow::getDpiScale() const
{
	return dpi_scale_;
}


bool SDL_ApplicationWindow::isVisible() const
{
	if (!window_)
		return false;
	
	Uint32 flags = SDL_GetWindowFlags(window_);
	return !(flags & SDL_WINDOW_HIDDEN);
}


// ========= Component Registration ======== //

void SDL_ApplicationWindow::Set_UIHandler(void* handler)
{
	ui_handler_ = handler;
}

void SDL_ApplicationWindow::setThreeDScreen(ThreeDScreen* screen)
{
	threed_screen_ = screen;
}

void SDL_ApplicationWindow::setVKContext(VKContext* context)
{
	vk_context_ = context;
	if (vulkan_pipelines_)
	{
		vulkan_pipelines_->setContext(context);
		std::cout << "[SDL_ApplicationWindow] VKContext propagated to VulkanPipeline" << std::endl;
	}
}

void SDL_ApplicationWindow::setVulkanPipelines(VulkanPipeline* pipelines)
{
	vulkan_pipelines_ = pipelines;
	if (vulkan_pipelines_ && vk_context_)
	{
		vulkan_pipelines_->setContext(vk_context_);
		std::cout << "[SDL_ApplicationWindow] Existing VKContext propagated to VulkanPipeline" << std::endl;
	}
	std::cout << "[SDL_ApplicationWindow] VulkanPipeline instance set" << std::endl;
}

void SDL_ApplicationWindow::updateFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas)
{
	frame_datas_ = frameDatas;
}


// ====== Event Handling ====== //

bool SDL_ApplicationWindow::handleSplitterEvent(const SDL_Event& event)
{
	if (!ui_handler_)
	{
		return false;
	}

	UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
	return handler->handleSplitterEvent(event);
}

void SDL_ApplicationWindow::processEvents()
{
	if (!window_)
		return;
	
	int currentX, currentY;
	SDL_GetWindowPosition(window_, &currentX, &currentY);
	
	if (currentX != last_x_ || currentY != last_y_)
	{
		last_x_ = currentX;
		last_y_ = currentY;
		std::cout << "[SDL_ApplicationWindow] Window moved to (" << currentX << ", " << currentY << ")" << std::endl;
	}
	
	int currentWidth, currentHeight;
	SDL_GetWindowSize(window_, &currentWidth, &currentHeight);
	
	if (currentWidth != last_width_ || currentHeight != last_height_)
	{
		std::cout << "\n ========================================" << std::endl;
		std::cout << "[SDL_ApplicationWindow] *** WINDOW RESIZE DETECTED ***" << std::endl;
		std::cout << "[SDL_ApplicationWindow] Old size: " << last_width_ << "x" << last_height_ << std::endl;
		std::cout << "[SDL_ApplicationWindow] New size: " << currentWidth << "x" << currentHeight << std::endl;
		std::cout << "========================================\n" << std::endl;
		
		last_width_ = currentWidth;
		last_height_ = currentHeight;
		swapchain_needs_recreation_ = true;		

		app_border_->updateDimensions(currentWidth, currentHeight);


	}
	
	bool currentMax = isMaximized();
	if (currentMax != is_maximized_)
	{
		is_maximized_ = currentMax;
		std::cout << "[SDL_ApplicationWindow] Window " << (is_maximized_ ? "MAXIMIZED" : "RESTORED") << std::endl;
		swapchain_needs_recreation_ = true;
	}

	if (app_border_)
	{
		BorderState borderState = app_border_->getCurrentState();
		
		static BorderState last_logged_state = BorderState::Init;
		if (borderState != last_logged_state)
		{
			std::string stateName;
			switch (borderState)
			{
				case BorderState::Init: stateName = "Init"; break;
				case BorderState::Maximized: stateName = "Maximized"; break;
				case BorderState::Reduced: stateName = "Reduced"; break;
				case BorderState::Updating: stateName = "Updating"; break;
			}
			std::cout << "[SDL_ApplicationWindow] App_Border state changed to: " << stateName << std::endl;
			last_logged_state = borderState;
		}
		
		switch (borderState)
		{
			case BorderState::Init:
				window_state_ = WindowRenderState::Init;
				break;
			case BorderState::Maximized:
				window_state_ = WindowRenderState::Maximized;
				break;
			case BorderState::Reduced:
				window_state_ = WindowRenderState::Reduced;
				break;
			case BorderState::Updating:
				window_state_ = WindowRenderState::Updating;
				break;
		}
	}

	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		handler->processPendingFrameUpdates();
	}
	
	updateUIState();
}

void SDL_ApplicationWindow::updateUIState()
{
	if (ui_manager_ && window_)
	{
		ui_manager_->updateUIState(window_);
	}
}


// ====== Rendering ====== //

void SDL_ApplicationWindow::renderFrame()
{
	static int render_frame_count = 0;
	render_frame_count++;
	
	if (render_frame_count <= 100)
	{
		std::cout << "[SDL_ApplicationWindow] renderFrame() START frame " << render_frame_count 
		          << " vk_context_=" << (vk_context_ != nullptr) 
		          << " swapchain=" << (vk_swapchain_ != VK_NULL_HANDLE) << std::endl;
	}
	else if (render_frame_count % 600 == 1)
	{
		std::cout << "[SDL_ApplicationWindow] renderFrame() called " << render_frame_count << " times" << std::endl;
	}
	
	if (!vk_context_ || vk_swapchain_ == VK_NULL_HANDLE)
	{
		if (render_frame_count <= 100 || render_frame_count % 600 == 1)
		{
			std::cout << "[SDL_ApplicationWindow] renderFrame() EARLY EXIT at frame " << render_frame_count 
			          << ": vk_context_=" << (vk_context_ != nullptr) 
			          << " swapchain=" << (vk_swapchain_ != VK_NULL_HANDLE) << std::endl;
		}
		return;
	}

	VkDevice device = vk_context_->getDevice();

	if (swapchain_needs_recreation_)
	{
		std::cout << "\n[SDL_ApplicationWindow] ========== SWAPCHAIN RECREATION START ==========" << std::endl;
		vkDeviceWaitIdle(device);
		
		vk_image_fences_.clear();
		
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vk_in_flight_fences_[i] != VK_NULL_HANDLE)
			{
				vkDestroyFence(device, vk_in_flight_fences_[i], nullptr);
			}
			if (vk_render_finished_semaphores_[i] != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, vk_render_finished_semaphores_[i], nullptr);
			}
			if (vk_image_available_semaphores_[i] != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(device, vk_image_available_semaphores_[i], nullptr);
			}
		}
		
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &vk_image_available_semaphores_[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &vk_render_finished_semaphores_[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, nullptr, &vk_in_flight_fences_[i]) != VK_SUCCESS)
			{
				std::cerr << "[SDL_ApplicationWindow] Failed to recreate sync objects" << std::endl;
				return;
			}
		}
		
		vk_image_available_semaphore_ = vk_image_available_semaphores_[0];
		vk_render_finished_semaphore_ = vk_render_finished_semaphores_[0];
		vk_in_flight_fence_ = vk_in_flight_fences_[0];
		
		current_frame_ = 0;
		current_image_index_ = 0;
		
		if (!recreateSwapchain())
		{
			std::cerr << "[SDL_ApplicationWindow] Failed to recreate swapchain" << std::endl;
			swapchain_needs_recreation_ = false;
			return;
		}
		
		swapchain_needs_recreation_ = false;
		std::cout << "[SDL_ApplicationWindow] ========== SWAPCHAIN RECREATION COMPLETE ==========\n" << std::endl;
		return;
	}

	vkWaitForFences(device, 1, &vk_in_flight_fences_[current_frame_], VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(device, vk_swapchain_, UINT64_MAX, vk_image_available_semaphores_[current_frame_], VK_NULL_HANDLE, &current_image_index_);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		std::cout << "[SDL_ApplicationWindow] Swapchain out of date during acquire, will recreate next frame" << std::endl;
		swapchain_needs_recreation_ = true;
		vkResetFences(device, 1, &vk_in_flight_fences_[current_frame_]);
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to acquire swapchain image (result=" << result << ")" << std::endl;
		vkResetFences(device, 1, &vk_in_flight_fences_[current_frame_]);
		return;
	}

	if (vk_image_fences_[current_image_index_] != VK_NULL_HANDLE)
	{
		vkWaitForFences(device, 1, &vk_image_fences_[current_image_index_], VK_TRUE, UINT64_MAX);
	}

	vkResetFences(device, 1, &vk_in_flight_fences_[current_frame_]);

	VkCommandBuffer commandBuffer = vk_command_buffers_[current_image_index_];
	vkResetCommandBuffer(commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to begin command buffer" << std::endl;
		return;
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vk_render_pass_;
	renderPassInfo.framebuffer = vk_framebuffers_[current_image_index_];
	renderPassInfo.renderArea.offset = {0, 0};

	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);
	renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

	VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	static int state_log_counter = 0;
	if (state_log_counter % 60 == 0)
	{
		std::string stateName;
		switch (window_state_)
		{
			case WindowRenderState::Init: stateName = "Init"; break;
			case WindowRenderState::Maximized: stateName = "Maximized"; break;
			case WindowRenderState::Reduced: stateName = "Reduced"; break;
			case WindowRenderState::Updating: stateName = "Updating"; break;
		}
		std::cout << "[SDL_ApplicationWindow] window_state_ = " << stateName << std::endl;
	}
	state_log_counter++;

	if (window_state_ == WindowRenderState::Init)
	{
		activateDebugRender();

		drawCEF();
		drawThreeDScreen();
		drawUIPanels();

		if (debug_tools_)
		{
			debug_tools_->drawDebugTools(commandBuffer, width, height);
		}
	}

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to end command buffer" << std::endl;
		return;
	}

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {vk_image_available_semaphores_[current_frame_]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkSemaphore signalSemaphores[] = {vk_render_finished_semaphores_[current_frame_]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, vk_in_flight_fences_[current_frame_]) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to submit draw command buffer" << std::endl;
		return;
	}

	vk_image_fences_[current_image_index_] = vk_in_flight_fences_[current_frame_];

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = {vk_swapchain_};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &current_image_index_;

	result = vkQueuePresentKHR(vk_context_->getPresentQueue(), &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		std::cout << "[SDL_ApplicationWindow] Swapchain " << (result == VK_ERROR_OUT_OF_DATE_KHR ? "out of date" : "suboptimal") << " after present, will recreate next frame" << std::endl;
		swapchain_needs_recreation_ = true;
	}
	else if (result != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to present swapchain image" << std::endl;
	}

	current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void SDL_ApplicationWindow::renderThreeDScreen(const std::map<std::string, IFrameData>&)
{
	if (threed_screen_)
	{
		threed_screen_->render(frame_datas_, window_);
	}
}

void SDL_ApplicationWindow::drawThreeDScreen()
{
	if (threed_screen_ && vk_command_buffers_.size() > current_image_index_)
	{
		threed_screen_->draw(vk_command_buffers_[current_image_index_], vk_render_pass_, vk_framebuffers_[current_image_index_]);
	}
}

void SDL_ApplicationWindow::drawUIPanels()
{
	static int draw_ui_call_count = 0;
	draw_ui_call_count++;
	
	if (draw_ui_call_count % 120 == 1) // Log every 2 seconds at 60 FPS 
	{
		std::cout << "[SDL_ApplicationWindow] drawUIPanels() called " << draw_ui_call_count << " times, ui_manager_=" << (ui_manager_ != nullptr) << std::endl;
	}
	
	if (!ui_manager_ || !vk_command_buffers_.size() > current_image_index_)
	{
		return;
	}

	int drawableWidth = 0;
	int drawableHeight = 0;
	SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight);

	VkCommandBuffer commandBuffer = vk_command_buffers_[current_image_index_];

	// Get panel frame data from splitter (via UIHandler) or from cached data
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> panelFrameDataMap;
	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		Splitter* splitter = handler->getSplitter();
	}

	if (panelFrameDataMap.empty())
	{
		panelFrameDataMap = ui_manager_->getUIPanelFrameDatas();
	}

	// Skip texture rebuild if splitter is dragging
	bool skipTextureRebuild = false;
	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		Splitter* splitter = handler->getSplitter();

	}

	ui_manager_->drawUIPanels(commandBuffer, drawableWidth, drawableHeight, panelFrameDataMap, skipTextureRebuild, window_);
}

void SDL_ApplicationWindow::drawCEF()
{
	if (ui_handler_ && vk_command_buffers_.size() > current_image_index_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		CEF_Drawer* cefDrawer = handler->getCEFDrawer();
		
		if (cefDrawer && cefDrawer->isInitialized())
		{
			static int call_count = 0;
			if (call_count++ < 3)
			{
				std::cout << "[SDL_ApplicationWindow::drawCEF] Calling cefDrawer->draw() with commandBuffer index " << current_image_index_ << std::endl;
			}
			cefDrawer->draw(vk_command_buffers_[current_image_index_]);
		}
		else
		{
			static int warning_count = 0;
			if (warning_count++ < 3)
			{
				std::cout << "[SDL_ApplicationWindow::drawCEF] WARNING: cefDrawer=" << cefDrawer 
				          << " isInitialized=" << (cefDrawer ? cefDrawer->isInitialized() : false) << std::endl;
			}
		}
	}
	else
	{
		static int handler_warning = 0;
		if (handler_warning++ < 3)
		{
			std::cout << "[SDL_ApplicationWindow::drawCEF] WARNING: ui_handler_=" << ui_handler_ 
			          << " command_buffers_size=" << vk_command_buffers_.size()
			          << " current_index=" << current_image_index_ << std::endl;
		}
	}
}

void SDL_ApplicationWindow::startSplitter()
{
	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		handler->startManager(vk_context_, vk_render_pass_);
	}

	if (debug_tools_ && vk_context_ && vk_render_pass_ != VK_NULL_HANDLE && vulkan_pipelines_ && app_border_)
	{
		debug_tools_->initialize(vk_context_, vk_render_pass_, vulkan_pipelines_, app_border_);
	}
}

void SDL_ApplicationWindow::activateDebugRender()
{
	if (debug_tools_)
	{
		debug_tools_->activateDebugRender();
	}
}



// ===== Vulkan Lifecycle ====== //

bool SDL_ApplicationWindow::initializeVulkan()
{
	if (!vk_context_)
	{
		std::cerr << "[SDL_ApplicationWindow] VKContext not set" << std::endl;
		return false;
	}

	std::cout << "[SDL_ApplicationWindow] Using VKContext surface..." << std::endl;
	vk_surface_ = vk_context_->getSurface();
	if (vk_surface_ == VK_NULL_HANDLE)
	{
		std::cerr << "[SDL_ApplicationWindow] VKContext surface is null" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Surface obtained from VKContext" << std::endl;

	if (!createSwapchain())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create swapchain" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Swapchain created" << std::endl;

	if (!createRenderPass())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create render pass" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Render pass created: " << vk_render_pass_ << std::endl;

	if (!createFramebuffers())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create framebuffers" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Framebuffers created" << std::endl;

	if (!createCommandPool())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create command pool" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Command pool created" << std::endl;

	if (!createCommandBuffers())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create command buffers" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Command buffers created" << std::endl;

	if (!createSyncObjects())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create sync objects" << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Sync objects created" << std::endl;

	std::cout << "[SDL_ApplicationWindow] Vulkan initialized successfully" << std::endl;
	return true;
}

void SDL_ApplicationWindow::cleanupVulkan()
{
	if (!vk_context_)
	{
		return;
	}

	VkDevice device = vk_context_->getDevice();
	
	vkDeviceWaitIdle(device);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (i < vk_in_flight_fences_.size() && vk_in_flight_fences_[i] != VK_NULL_HANDLE)
		{
			vkDestroyFence(device, vk_in_flight_fences_[i], nullptr);
		}
		if (i < vk_render_finished_semaphores_.size() && vk_render_finished_semaphores_[i] != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, vk_render_finished_semaphores_[i], nullptr);
		}
		if (i < vk_image_available_semaphores_.size() && vk_image_available_semaphores_[i] != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(device, vk_image_available_semaphores_[i], nullptr);
		}
	}
	vk_in_flight_fences_.clear();
	vk_render_finished_semaphores_.clear();
	vk_image_available_semaphores_.clear();
	vk_image_fences_.clear();

	vk_in_flight_fence_ = VK_NULL_HANDLE;
	vk_render_finished_semaphore_ = VK_NULL_HANDLE;
	vk_image_available_semaphore_ = VK_NULL_HANDLE;

	if (vk_command_pool_ != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(device, vk_command_pool_, nullptr);
		vk_command_pool_ = VK_NULL_HANDLE;
	}

	for (auto framebuffer : vk_framebuffers_)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	vk_framebuffers_.clear();

	if (vk_render_pass_ != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(device, vk_render_pass_, nullptr);
		vk_render_pass_ = VK_NULL_HANDLE;
	}

	for (auto imageView : vk_swapchain_image_views_)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}
	vk_swapchain_image_views_.clear();

	if (vk_swapchain_ != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, vk_swapchain_, nullptr);
		vk_swapchain_ = VK_NULL_HANDLE;
	}

	vk_surface_ = VK_NULL_HANDLE;
}


bool SDL_ApplicationWindow::recreateSwapchain()
{
	if (!vk_context_)
	{
		std::cerr << "[SDL_ApplicationWindow] recreateSwapchain() - No VKContext" << std::endl;
		return false;
	}

	int drawableWidth, drawableHeight;
	SDL_GetWindowSizeInPixels(window_, &drawableWidth, &drawableHeight);
	
	int logicalWidth, logicalHeight;
	SDL_GetWindowSize(window_, &logicalWidth, &logicalHeight);
	
	std::cout << "[SDL_ApplicationWindow] recreateSwapchain() - New size (drawable): " << drawableWidth << "x" << drawableHeight << std::endl;
	std::cout << "[SDL_ApplicationWindow] recreateSwapchain() - New size (logical): " << logicalWidth << "x" << logicalHeight << std::endl;

	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		handler->updateWindowSize(logicalWidth, logicalHeight);
	}

	VkDevice device = vk_context_->getDevice();

	for (auto framebuffer : vk_framebuffers_)
	{
		vkDestroyFramebuffer(device, framebuffer, nullptr);
	}
	vk_framebuffers_.clear();

	for (auto imageView : vk_swapchain_image_views_)
	{
		vkDestroyImageView(device, imageView, nullptr);
	}
	vk_swapchain_image_views_.clear();

	if (vk_swapchain_ != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, vk_swapchain_, nullptr);
		vk_swapchain_ = VK_NULL_HANDLE;
	}

	if (!createSwapchain())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to recreate swapchain" << std::endl;
		return false;
	}

	vk_image_fences_.clear();
	vk_image_fences_.resize(vk_swapchain_images_.size(), VK_NULL_HANDLE);

	if (!createFramebuffers())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to recreate framebuffers" << std::endl;
		return false;
	}

	std::cout << "[SDL_ApplicationWindow] Swapchain and framebuffers recreated successfully" << std::endl;
	
	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		CEF_Drawer* cefDrawer = handler->getCEFDrawer();
		if (cefDrawer)
		{
			std::cout << "[SDL_ApplicationWindow] Syncing CEF window dimensions (logical): " << logicalWidth << "x" << logicalHeight << std::endl;
			cefDrawer->getResizer().resize(logicalWidth, logicalHeight);
			
			std::cout << "[SDL_ApplicationWindow] Forcing CEF repaint with new window size" << std::endl;

			
			std::cout << "[SDL_ApplicationWindow] Syncing CEF window properties after swapchain recreation" << std::endl;
			cefDrawer->syncWindowProperties();
		}
		
		Splitter* splitter = handler->getSplitter();
		if (splitter)
		{
			std::cout << "[SDL_ApplicationWindow] Forcing splitter layout refresh after swapchain recreation" << std::endl;
			// splitter->forceRefreshLayout();
		}
	}
	
	return true;
}



// ==== private methods ==== //

void SDL_ApplicationWindow::updateDpiScale()
{
	std::cout << "[SDL_ApplicationWindow] updateDpiScale() entry" << std::endl;
	
	if (!window_)
	{
		std::cout << "[SDL_ApplicationWindow] No window, setting DPI scale to 1.0" << std::endl;
		dpi_scale_ = 1.0f;
		return;
	}
	
	std::cout << "[SDL_ApplicationWindow] Getting display for window..." << std::endl;
	SDL_DisplayID displayID = SDL_GetDisplayForWindow(window_);
	std::cout << "[SDL_ApplicationWindow] DisplayID: " << displayID << std::endl;
	
	if (displayID != 0)
	{
		std::cout << "[SDL_ApplicationWindow] Getting content scale..." << std::endl;
		dpi_scale_ = SDL_GetDisplayContentScale(displayID);
		std::cout << "[SDL_ApplicationWindow] Content scale obtained: " << dpi_scale_ << std::endl;
	}
	else
	{
		std::cout << "[SDL_ApplicationWindow] Invalid display, setting DPI scale to 1.0" << std::endl;
		dpi_scale_ = 1.0f;
	}
	
	std::cout << "[SDL_ApplicationWindow] updateDpiScale() exit" << std::endl;
}

void SDL_ApplicationWindow::updateMaximizedState()
{
	is_maximized_ = isMaximized();
}

void SDL_ApplicationWindow::captureFrameData()
{
	if (ui_handler_)
	{
		UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		handler->forceCaptureIFramePositions();
	}
}


bool SDL_ApplicationWindow::createSwapchain()
{
	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);

	std::cout << "[SDL_ApplicationWindow] Creating swapchain for " << width << "x" << height << std::endl;

	VkSurfaceCapabilitiesKHR capabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_context_->getPhysicalDevice(), vk_surface_, &capabilities);
	if (result != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to get surface capabilities" << std::endl;
		return false;
	}

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context_->getPhysicalDevice(), vk_surface_, &formatCount, nullptr);
	if (formatCount == 0)
	{
		std::cerr << "[SDL_ApplicationWindow] No surface formats available" << std::endl;
		return false;
	}
	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_context_->getPhysicalDevice(), vk_surface_, &formatCount, formats.data());

	VkSurfaceFormatKHR surfaceFormat = formats[0];
	std::cout << "[SDL_ApplicationWindow] Using surface format: " << surfaceFormat.format << std::endl;

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

	VkExtent2D extent;
	extent.width = static_cast<uint32_t>(width);
	extent.height = static_cast<uint32_t>(height);

	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
	{
		imageCount = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = vk_surface_;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	std::cout << "[SDL_ApplicationWindow] Creating swapchain..." << std::endl;
	VkResult swapchainResult = vkCreateSwapchainKHR(vk_context_->getDevice(), &createInfo, nullptr, &vk_swapchain_);
	if (swapchainResult != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create swapchain, error code: " << swapchainResult << std::endl;
		return false;
	}
	std::cout << "[SDL_ApplicationWindow] Swapchain created successfully" << std::endl;

	vkGetSwapchainImagesKHR(vk_context_->getDevice(), vk_swapchain_, &imageCount, nullptr);
	vk_swapchain_images_.resize(imageCount);
	vkGetSwapchainImagesKHR(vk_context_->getDevice(), vk_swapchain_, &imageCount, vk_swapchain_images_.data());

	vk_swapchain_image_views_.resize(vk_swapchain_images_.size());
	for (size_t i = 0; i < vk_swapchain_images_.size(); i++)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vk_swapchain_images_[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = surfaceFormat.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(vk_context_->getDevice(), &viewInfo, nullptr, &vk_swapchain_image_views_[i]) != VK_SUCCESS)
		{
			return false;
		}
	}

	std::cout << "[SDL_ApplicationWindow] createSwapchain() SUCCESS: vk_swapchain_=" << vk_swapchain_ 
	          << " imageCount=" << imageCount << std::endl;
	return true;
}

bool SDL_ApplicationWindow::createRenderPass()
{
	std::cout << "[SDL_ApplicationWindow::createRenderPass] Creating render pass..." << std::endl;
	
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VkResult result = vkCreateRenderPass(vk_context_->getDevice(), &renderPassInfo, nullptr, &vk_render_pass_);
	if (result != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow::createRenderPass] Failed with error code: " << result << std::endl;
		return false;
	}
	
	std::cout << "[SDL_ApplicationWindow::createRenderPass] Render pass created successfully: " << vk_render_pass_ << std::endl;
	return true;
}

bool SDL_ApplicationWindow::createFramebuffers()
{
	vk_framebuffers_.resize(vk_swapchain_image_views_.size());

	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);

	for (size_t i = 0; i < vk_swapchain_image_views_.size(); i++)
	{
		VkImageView attachments[] = {vk_swapchain_image_views_[i]};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = vk_render_pass_;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = static_cast<uint32_t>(width);
		framebufferInfo.height = static_cast<uint32_t>(height);
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vk_context_->getDevice(), &framebufferInfo, nullptr, &vk_framebuffers_[i]) != VK_SUCCESS)
		{
			return false;
		}
	}

	return true;
}

bool SDL_ApplicationWindow::createCommandPool()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = vk_context_->getGraphicsQueueFamily();

	if (vkCreateCommandPool(vk_context_->getDevice(), &poolInfo, nullptr, &vk_command_pool_) != VK_SUCCESS)
	{
		return false;
	}

	return true;
}

bool SDL_ApplicationWindow::createCommandBuffers()
{
	vk_command_buffers_.resize(vk_framebuffers_.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk_command_pool_;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(vk_command_buffers_.size());

	if (vkAllocateCommandBuffers(vk_context_->getDevice(), &allocInfo, vk_command_buffers_.data()) != VK_SUCCESS)
	{
		return false;
	}

	return true;
}

bool SDL_ApplicationWindow::createSyncObjects()
{
	vk_image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
	vk_render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
	vk_in_flight_fences_.resize(MAX_FRAMES_IN_FLIGHT);
	vk_image_fences_.resize(vk_swapchain_images_.size(), VK_NULL_HANDLE);
	
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(vk_context_->getDevice(), &semaphoreInfo, nullptr, &vk_image_available_semaphores_[i]) != VK_SUCCESS ||
			vkCreateSemaphore(vk_context_->getDevice(), &semaphoreInfo, nullptr, &vk_render_finished_semaphores_[i]) != VK_SUCCESS ||
			vkCreateFence(vk_context_->getDevice(), &fenceInfo, nullptr, &vk_in_flight_fences_[i]) != VK_SUCCESS)
		{
			return false;
		}
	}

	vk_image_available_semaphore_ = vk_image_available_semaphores_[0];
	vk_render_finished_semaphore_ = vk_render_finished_semaphores_[0];
	vk_in_flight_fence_ = vk_in_flight_fences_[0];

	return true;
}