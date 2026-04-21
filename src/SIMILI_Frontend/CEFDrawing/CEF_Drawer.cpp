#include "CEF_Drawer.hpp"
#include "CEF_Resizer.hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include "../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <array>
#include "include/base/cef_callback.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

CEF_Drawer* CEF_Drawer::active_instance_ = nullptr;

CEF_Drawer::CEF_Drawer()
	: window_(nullptr)
	, vk_context_(nullptr)
	, vulkan_pipelines_(nullptr)
	, vk_render_pass_(VK_NULL_HANDLE)
	, texture_image_(VK_NULL_HANDLE)
	, texture_memory_(VK_NULL_HANDLE)
	, texture_view_(VK_NULL_HANDLE)
	, texture_sampler_(VK_NULL_HANDLE)
	, vertex_buffer_(VK_NULL_HANDLE)
	, vertex_buffer_memory_(VK_NULL_HANDLE)
	, vertex_shader_(VK_NULL_HANDLE)
	, fragment_shader_(VK_NULL_HANDLE)
	, pipeline_(VK_NULL_HANDLE)
	, pipeline_layout_(VK_NULL_HANDLE)
	, descriptor_pool_(VK_NULL_HANDLE)
	, descriptor_set_layout_(VK_NULL_HANDLE)
	, descriptor_set_(VK_NULL_HANDLE)
	, width_(1920)
	, height_(1080)
	, logical_width_(0)
	, logical_height_(0)
	, drawable_width_(0)
	, drawable_height_(0)
	, dpi_scale_(1.0f)
	, initialized_(false)
	, texture_layout_initialized_(false)
	, browser_(nullptr)
	, url_("")
	, paint_buffer_width_(0)
	, paint_buffer_height_(0)
	, runtime_layout_sync_pending_(false)
	, runtime_layout_waiting_for_paint_(false)
	, runtime_layout_needs_second_invalidate_(false)
	, has_received_first_paint_(false)
	, is_initialized_render_complete_(false)
	, suppress_cef_repaints_(false)
	, force_single_repaint_(false)
	, initial_paint_count_(0)
	, descriptor_needs_update_(false)
	, last_bound_texture_view_(VK_NULL_HANDLE)
	, paint_buffer_synchronized_(false)
	, preserve_textures_during_resize_(false)
	, resize_wait_frames_(0)
	, resizer_(std::make_unique<CEF_Resizer>(*this))
{
}

CEF_Drawer::~CEF_Drawer()
{
	shutdown();
}

CEF_Resizer& CEF_Drawer::getResizer()
{
	return *resizer_;
}

bool CEF_Drawer::initialize(SDL_Window* window, VKContext* vkContext, VulkanPipeline* vulkanPipelines)
{
	if (initialized_)
	{
		std::cerr << "[CEF_Drawer] Already initialized" << std::endl;
		return false;
	}
	
	if (!window || !vkContext || !vulkanPipelines)
	{
		std::cerr << "[CEF_Drawer] Invalid SDL window, VKContext, or VulkanPipeline" << std::endl;
		return false;
	}
	
	std::cout << "[CEF_Drawer] Starting initialization..." << std::endl;
	
	window_ = window;
	vk_context_ = vkContext;
	vulkan_pipelines_ = vulkanPipelines;
	updateWindowProperties();
	width_ = logical_width_;
	height_ = logical_height_;
	
	std::cout << "[CEF_Drawer] Creating Vulkan resources..." << std::endl;
	if (!createVulkanResources())
	{
		std::cerr << "[CEF_Drawer] Failed to create Vulkan resources" << std::endl;
		shutdown();
		return false;
	}
	
	initialized_ = true;
	active_instance_ = this;
	std::cout << "[CEF_Drawer] Initialized (logical pixels): " << width_ << "x" << height_ << std::endl;
	std::cout << "[CEF_Drawer] Pipeline status: " << (pipeline_ != VK_NULL_HANDLE ? "CREATED" : "NOT CREATED (waiting for render pass)") << std::endl;
	
	return true;
}

void CEF_Drawer::syncWindowProperties()
{
	if (!initialized_ || !window_)
	{
		return;
	}

	updateWindowProperties();

	int newWidth = logical_width_;
	int newHeight = logical_height_;

	if (newWidth <= 0 || newHeight <= 0 || (newWidth == width_ && newHeight == height_))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		paint_buffer_synchronized_ = false;
		
		if (!preserve_textures_during_resize_)
		{
			preserve_textures_during_resize_ = true;
			resize_wait_frames_ = 10;
			std::cout << "[CEF_Drawer::syncWindowProperties] Activating texture preservation for resize: " << width_ << "x" << height_ << " -> " << newWidth << "x" << newHeight << std::endl;
		}
		else
		{
			resize_wait_frames_ = 10;
			std::cout << "[CEF_Drawer::syncWindowProperties] Resize already in progress - resetting timeout" << std::endl;
		}
		
		width_ = newWidth;
		height_ = newHeight;
		resizer_->ensureTextureStorage(width_, height_);
		
		if (is_initialized_render_complete_)
		{
			force_single_repaint_ = true;
		}
	}

	if (browser_)
	{
		CefRefPtr<CefBrowserHost> host = browser_->GetHost();
		if (host)
		{
			host->WasResized();
			if (!is_initialized_render_complete_ || force_single_repaint_)
			{
				host->Invalidate(PET_VIEW);
			}
		}
	}

	std::cout << "[CEF_Drawer] Synced SDL window properties: " << width_ << "x" << height_ << std::endl;
}

void CEF_Drawer::setRenderPass(VkRenderPass renderPass)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	std::cout << "[CEF_Drawer] setRenderPass called with renderPass=" << renderPass << std::endl;
	
	if (renderPass == VK_NULL_HANDLE)
	{
		std::cerr << "[CEF_Drawer] ERROR: Render pass is VK_NULL_HANDLE!" << std::endl;
		return;
	}
	
	vk_render_pass_ = renderPass;
	std::cout << "[CEF_Drawer] Render pass set: " << renderPass << std::endl;
	
	// Release shared pipeline (will be recreated with new render pass)
	if (shared_pipeline_)
	{
		std::cout << "[CEF_Drawer] Releasing old pipeline (shared_ptr)" << std::endl;
		shared_pipeline_.reset();
		pipeline_ = VK_NULL_HANDLE;  // Clear deprecated handle for consistency
	}
	
	std::cout << "[CEF_Drawer] About to create pipeline with render pass: " << vk_render_pass_ << std::endl;
	std::cout << "[CEF_Drawer] Checking prerequisites:" << std::endl;
	std::cout << "  - texture_view_: " << texture_view_ << std::endl;
	std::cout << "  - texture_sampler_: " << texture_sampler_ << std::endl;
	std::cout << "  - vertex_buffer_: " << vertex_buffer_ << std::endl;
	std::cout << "  - vulkan_pipelines_: " << vulkan_pipelines_ << std::endl;
	
	if (texture_view_ == VK_NULL_HANDLE || texture_sampler_ == VK_NULL_HANDLE)
	{
		std::cerr << "[CEF_Drawer] ERROR: Texture resources not created!" << std::endl;
		return;
	}

	if (!vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
	{
		std::cerr << "[CEF_Drawer] ERROR: VulkanPipeline system is not initialized" << std::endl;
		return;
	}
	
	if (!createVulkanPipeline())
	{
		std::cerr << "[CEF_Drawer] Failed to create Vulkan pipeline with render pass" << std::endl;
	}
	else
	{
		std::cout << "[CEF_Drawer] Pipeline created successfully with render pass, shared_pipeline=" 
		 << (shared_pipeline_ ? shared_pipeline_->pipeline : VK_NULL_HANDLE) << std::endl;
	}
}

bool CEF_Drawer::createBrowser(CefRefPtr<CefClient> client, const std::string& url, int width, int height)
{
	if (browser_)
	{
		browser_->GetHost()->CloseBrowser(true);
		browser_ = nullptr;
	}
	
	if (!initialized_)
	{
		std::cerr << "[CEF_Drawer] Cannot create browser: not initialized" << std::endl;
		return false;
	}
	
	if (browser_)
	{
		std::cerr << "[CEF_Drawer] Browser already created" << std::endl;
		return false;
	}
	
	url_ = url;
	width_ = width;
	height_ = height;
	updateWindowProperties();
	
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	browser_settings.javascript = STATE_ENABLED;
	browser_settings.javascript_close_windows = STATE_ENABLED;
	browser_settings.javascript_access_clipboard = STATE_ENABLED;
	browser_settings.javascript_dom_paste = STATE_ENABLED;
	
	CefWindowInfo window_info;
	window_info.SetAsWindowless(0);
	
	browser_ = CefBrowserHost::CreateBrowserSync(window_info, client, url_, browser_settings, nullptr, nullptr);
	
	if (!browser_)
	{
		std::cerr << "[CEF_Drawer] Failed to create CEF browser" << std::endl;
		return false;
	}
	
	std::cout << "[CEF_Drawer] Browser created with URL: " << url_ << " (logical pixels): " << width_ << "x" << height_ << std::endl;
	
	CefRefPtr<CefBrowserHost> host = browser_->GetHost();
	if (host)
	{
		host->WasResized();
		host->Invalidate(PET_VIEW);
		std::cout << "[CEF_Drawer] Browser invalidated to trigger initial render" << std::endl;
	}
	
	return true;
}

void CEF_Drawer::shutdown()
{
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		ui_panel_frames_.clear();
		ui_panel_display_frames_.clear();
		runtime_layout_frames_.clear();
		paint_buffer_.clear();
		paint_buffer_width_ = 0;
		paint_buffer_height_ = 0;
		runtime_layout_sync_pending_ = false;
		runtime_layout_waiting_for_paint_ = false;
		has_received_first_paint_ = false;
		paint_buffer_synchronized_ = false;
		for (auto& texturePair : ui_panel_textures_)
		{
			if (texturePair.second.texture_view != VK_NULL_HANDLE)
			{
				vkDestroyImageView(vk_context_->getDevice(), texturePair.second.texture_view, nullptr);
				texturePair.second.texture_view = VK_NULL_HANDLE;
			}
			if (texturePair.second.texture_image != VK_NULL_HANDLE)
			{
				vkDestroyImage(vk_context_->getDevice(), texturePair.second.texture_image, nullptr);
				texturePair.second.texture_image = VK_NULL_HANDLE;
			}
			if (texturePair.second.texture_memory != VK_NULL_HANDLE)
			{
				vkFreeMemory(vk_context_->getDevice(), texturePair.second.texture_memory, nullptr);
				texturePair.second.texture_memory = VK_NULL_HANDLE;
			}
		}
		ui_panel_textures_.clear();
		if (active_instance_ == this)
		{
			active_instance_ = nullptr;
		}
	}

	cleanupVulkanResources();
	
	initialized_ = false;
	std::cout << "[CEF_Drawer] Shutdown complete" << std::endl;
}

void CEF_Drawer::draw(VkCommandBuffer commandBuffer)
{
	static int draw_call_count = 0;
	static bool initialization_logged = false;
	static bool skip_fullscreen_warning_logged = false;
	
	syncWindowProperties();
	
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		if (preserve_textures_during_resize_ && resize_wait_frames_ > 0)
		{
			resize_wait_frames_--;
			if (resize_wait_frames_ == 0)
			{
				std::cout << "[CEF_Drawer::draw] Resize timeout reached - waiting for synchronized OnPaint" << std::endl;
			}
		}
	}
	
	if (!initialized_ || !vk_context_)
	{
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] draw() early return: initialized=" << initialized_ 
					  << " vk_context=" << (vk_context_ != nullptr) << std::endl;
		}
		return;
	}
	
	if (!ui_panel_frames_.empty())
	{
		if (!skip_fullscreen_warning_logged)
		{
			std::cout << "[CEF_Drawer] draw() SKIPPED - UI panels are active (" << ui_panel_frames_.size() << " panels), fullscreen CEF rendering disabled" << std::endl;
			skip_fullscreen_warning_logged = true;
		}
		return;
	}
	
	if (!has_received_first_paint_)
	{
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] draw() waiting for first OnPaint from CEF" << std::endl;
		}
		return;
	}
	
	if (paint_buffer_.empty())
	{
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] draw() paint buffer is empty" << std::endl;
		}
		return;
	}
	
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	if (!shared_pipeline_ || shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] draw() shared_pipeline is NULL or invalid" << std::endl;
		}
		return;
	}
	
	if (texture_view_ == VK_NULL_HANDLE || vertex_buffer_ == VK_NULL_HANDLE)
	{
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] draw() texture_view or vertex_buffer is NULL" << std::endl;
		}
		return;
	}
	
	if (descriptor_set_ == VK_NULL_HANDLE || texture_sampler_ == VK_NULL_HANDLE)
	{
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] draw() descriptor_set or texture_sampler is NULL" << std::endl;
		}
		return;
	}
	
	if (descriptor_needs_update_ || last_bound_texture_view_ != texture_view_)
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = texture_view_;
		imageInfo.sampler = texture_sampler_;
		
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptor_set_;
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;
		
		vkUpdateDescriptorSets(vk_context_->getDevice(), 1, &descriptorWrite, 0, nullptr);
		
		descriptor_needs_update_ = false;
		last_bound_texture_view_ = texture_view_;
		
		if (!initialization_logged)
		{
			std::cout << "[CEF_Drawer] Descriptor set updated with texture_view=" << texture_view_ << std::endl;
		}
	}
	
	if (!initialization_logged)
	{
		std::cout << "[CEF_Drawer] First successful draw() with shared_pipeline=" << shared_pipeline_->pipeline << std::endl;
		initialization_logged = true;
	}
	
	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);
	
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
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shared_pipeline_->pipeline);
	
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		shared_pipeline_->layout, 0, 1, &descriptor_set_, 0, nullptr);
	
	VkBuffer vertexBuffers[] = {vertex_buffer_};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	draw_call_count++;
}

void CEF_Drawer::updateUIPanelFrames(const std::map<std::string, UIPanelFrameData>& panelFrames)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	for (const auto& newFramePair : panelFrames)
	{
		auto oldIt = ui_panel_frames_.find(newFramePair.first);
		bool needsUpdate = false;
		
		if (oldIt == ui_panel_frames_.end())
		{
			needsUpdate = true;
		}
		else
		{
			const UIPanelFrameData& oldFrame = oldIt->second;
			const UIPanelFrameData& newFrame = newFramePair.second;
			
			if (oldFrame.x != newFrame.x || oldFrame.y != newFrame.y ||
			    oldFrame.width != newFrame.width || oldFrame.height != newFrame.height)
			{
				needsUpdate = true;
			}
		}
		
		if (needsUpdate)
		{
			auto textureIt = ui_panel_textures_.find(newFramePair.first);
			if (textureIt != ui_panel_textures_.end())
			{
				textureIt->second.dirty = true;
			}
		}
	}
	
	ui_panel_frames_ = panelFrames;

	for (auto it = ui_panel_display_frames_.begin(); it != ui_panel_display_frames_.end();)
	{
		if (ui_panel_frames_.find(it->first) == ui_panel_frames_.end())
		{
			it = ui_panel_display_frames_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void CEF_Drawer::updateUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	ui_panel_display_frames_[panelName] = panelFrame;
	// Do NOT sync to ui_panel_frames_ - display uses SDL drawable coords, frames use CEF logical coords
}

void CEF_Drawer::updateUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	ui_panel_frames_[panelName] = sourceFrame;
}

bool CEF_Drawer::getUIPanelTextureRegion(const std::string& panelName, VkImageView& outTextureView, VkSampler& outSampler, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	auto it = ui_panel_frames_.find(panelName);
	if (it == ui_panel_frames_.end())
	{
		std::cout << "[CEF_Drawer::getUIPanelTextureRegion] " << panelName << " - Not found in ui_panel_frames_" << std::endl;
		return false;
	}

	if (it->second.width <= 0 || it->second.height <= 0)
	{
		std::cout << "[CEF_Drawer::getUIPanelTextureRegion] " << panelName << " - Invalid dimensions: " << it->second.width << "x" << it->second.height << std::endl;
		return false;
	}

	if (texture_sampler_ == VK_NULL_HANDLE)
	{
		std::cout << "[CEF_Drawer::getUIPanelTextureRegion] " << panelName << " - texture_sampler_ is NULL, CEF resources not ready" << std::endl;
		return false;
	}

	UIPanelTextureData& textureData = ui_panel_textures_[panelName];
	
	bool needsRebuild = textureData.texture_view == VK_NULL_HANDLE || textureData.dirty;
	
	if (needsRebuild && !resizer_->rebuildUIPanelTextureLocked(panelName, textureData, outFrame))
	{
		std::cout << "[CEF_Drawer::getUIPanelTextureRegion] " << panelName << " - rebuildUIPanelTextureLocked failed" << std::endl;
		return false;
	}
	else if (!needsRebuild)
	{
		outFrame = it->second;
	}

	if (textureData.texture_view == VK_NULL_HANDLE)
	{
		std::cout << "[CEF_Drawer::getUIPanelTextureRegion] " << panelName << " - textureData.texture_view is NULL after rebuild" << std::endl;
		return false;
	}

	outTextureView = textureData.texture_view;
	outSampler = texture_sampler_;
	outTextureWidth = textureData.width;
	outTextureHeight = textureData.height;
	
	if (needsRebuild)
	{
		std::cout << "[CEF_Drawer::getUIPanelTextureRegion] " << panelName << " - SUCCESS: rebuilt texture " << outTextureWidth << "x" << outTextureHeight << std::endl;
	}
	
	return true;
}

bool CEF_Drawer::getActiveUIPanelTextureRegion(const std::string& panelName, VkImageView& outTextureView, VkSampler& outSampler, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame)
{
	if (!active_instance_)
	{
		return false;
	}

	return active_instance_->getUIPanelTextureRegion(panelName, outTextureView, outSampler, outTextureWidth, outTextureHeight, outFrame);
}

bool CEF_Drawer::isUIPanelTextureDirty(const std::string& panelName)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	auto it = ui_panel_textures_.find(panelName);
	if (it == ui_panel_textures_.end())
	{
		return true;
	}

	return it->second.dirty;
}

bool CEF_Drawer::isActiveUIPanelTextureDirty(const std::string& panelName)
{
	if (!active_instance_)
	{
		return false;
	}

	return active_instance_->isUIPanelTextureDirty(panelName);
}

void CEF_Drawer::updateActiveUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame)
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->updateUIPanelDisplayFrame(panelName, panelFrame);
}

void CEF_Drawer::updateActiveUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame)
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->updateUIPanelSourceFrame(panelName, sourceFrame);
}

void CEF_Drawer::requestActiveRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames)
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->requestRuntimeLayoutSync(panelFrames);
}

void CEF_Drawer::forceActiveLayoutSync()
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->getResizer().forceLayoutSync();
}

void CEF_Drawer::requestRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames)
{
	if (panelFrames.empty())
	{
		return;
	}

	bool shouldSchedule = false;
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		runtime_layout_frames_ = panelFrames;
		runtime_layout_waiting_for_paint_ = true;
		runtime_layout_needs_second_invalidate_ = true;

		if (!runtime_layout_sync_pending_)
		{
			runtime_layout_sync_pending_ = true;
			shouldSchedule = true;
		}
	}

	if (!shouldSchedule)
	{
		return;
	}

	if (CefCurrentlyOn(TID_UI))
	{
		resizer_->flushRuntimeLayoutSync();
		return;
	}

	CefPostTask(TID_UI, base::BindOnce(&CEF_Resizer::flushRuntimeLayoutSync, base::Unretained(resizer_.get())));
}

void CEF_Drawer::invalidateAllUIPanelTextures()
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	if (preserve_textures_during_resize_)
	{
		std::cout << "[CEF_Drawer] Invalidation skipped - preserving textures during resize" << std::endl;
		return;
	}
	
	for (auto& texturePair : ui_panel_textures_)
	{
		texturePair.second.dirty = true;
	}
	std::cout << "[CEF_Drawer] Invalidated all UI panel textures count=" << ui_panel_textures_.size() << std::endl;
}

void CEF_Drawer::handleEvent(const SDL_Event& event)
{
	if (!browser_)
		return;
	
	CefRefPtr<CefBrowserHost> host = browser_->GetHost();
	if (!host)
		return;
	
	switch (event.type)
	{
		case SDL_EVENT_MOUSE_MOTION:
		{
			CefMouseEvent mouse_event;
			resizer_->translateMousePosition(event.motion.x, event.motion.y, mouse_event.x, mouse_event.y);
			mouse_event.modifiers = GetCefModifiers(event);
			host->SendMouseMoveEvent(mouse_event, false);
			break;
		}
		
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			CefMouseEvent mouse_event;
			resizer_->translateMousePosition(event.button.x, event.button.y, mouse_event.x, mouse_event.y);
			mouse_event.modifiers = GetCefModifiers(event);
			
			CefBrowserHost::MouseButtonType button_type = MBT_LEFT;
			if (event.button.button == SDL_BUTTON_LEFT)
				button_type = MBT_LEFT;
			else if (event.button.button == SDL_BUTTON_RIGHT)
				button_type = MBT_RIGHT;
			else if (event.button.button == SDL_BUTTON_MIDDLE)
				button_type = MBT_MIDDLE;
			
			bool is_down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
			host->SendMouseClickEvent(mouse_event, button_type, !is_down, 1);
			
			// Send focus on mouse down
			if (is_down)
			{
				host->SetFocus(true);
			}
			break;
		}
		
		case SDL_EVENT_MOUSE_WHEEL:
		{
			CefMouseEvent mouse_event;
			// Use current mouse position (SDL doesn't provide it in wheel event)
			float mouseX, mouseY;
			SDL_GetMouseState(&mouseX, &mouseY);
			resizer_->translateMousePosition(mouseX, mouseY, mouse_event.x, mouse_event.y);
			mouse_event.modifiers = GetCefModifiers(event);
			
			// SDL3 wheel values are float, convert to pixels (multiply by ~100 for smooth scrolling)
			int deltaX = static_cast<int>(event.wheel.x * 100.0f);
			int deltaY = static_cast<int>(event.wheel.y * 100.0f);
			
			host->SendMouseWheelEvent(mouse_event, deltaX, deltaY);
			break;
		}
		
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			CefKeyEvent key_event;
			key_event.windows_key_code = GetWindowsKeyCode(event.key.scancode, event.key.key);
			key_event.native_key_code = event.key.scancode;
			key_event.modifiers = GetCefKeyboardModifiers(event);
			
			if (event.type == SDL_EVENT_KEY_DOWN)
			{
				key_event.type = KEYEVENT_RAWKEYDOWN;
			}
			else
			{
				key_event.type = KEYEVENT_KEYUP;
			}
			
			host->SendKeyEvent(key_event);
			
			// For printable characters, send CHAR event
			if (event.type == SDL_EVENT_KEY_DOWN)
			{
				// Check if it's a printable character
				if (event.key.key >= 32 && event.key.key < 127)
				{
					CefKeyEvent char_event = key_event;
					char_event.type = KEYEVENT_CHAR;
					char_event.windows_key_code = event.key.key;
					host->SendKeyEvent(char_event);
				}
			}
			break;
		}
		
		case SDL_EVENT_TEXT_INPUT:
		{
			// Handle text input for complex input methods (IME, etc.)
			const char* text = event.text.text;
			for (const char* p = text; *p != 0; ++p)
			{
				CefKeyEvent key_event;
				key_event.type = KEYEVENT_CHAR;
				key_event.windows_key_code = *p;
				key_event.character = *p;
				key_event.unmodified_character = *p;
				key_event.modifiers = 0;
				host->SendKeyEvent(key_event);
			}
			break;
		}
		
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		{
			host->SetFocus(true);
			break;
		}
		
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		{
			host->SetFocus(false);
			break;
		}
		
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		{
			syncWindowProperties();
			break;
		}
	}
}

CEF_Drawer::SDLWindowProperties CEF_Drawer::getSDLWindowProperties()
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	SDLWindowProperties properties;
	properties.logical_width = logical_width_;
	properties.logical_height = logical_height_;
	properties.drawable_width = drawable_width_;
	properties.drawable_height = drawable_height_;
	properties.dpi_scale = dpi_scale_;
	return properties;
}

uint32_t CEF_Drawer::GetCefModifiers(const SDL_Event& event)
{
	uint32_t modifiers = 0;
	SDL_Keymod mod = SDL_GetModState();
	
	if (mod & SDL_KMOD_SHIFT)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (mod & SDL_KMOD_CTRL)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (mod & SDL_KMOD_ALT)
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (mod & SDL_KMOD_GUI)
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	
	// Mouse button modifiers
	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || 
	    event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
	    event.type == SDL_EVENT_MOUSE_MOTION)
	{
		Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
		if (buttons & SDL_BUTTON_LMASK)
			modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
		if (buttons & SDL_BUTTON_RMASK)
			modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
		if (buttons & SDL_BUTTON_MMASK)
			modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	}
	
	return modifiers;
}

uint32_t CEF_Drawer::GetCefKeyboardModifiers(const SDL_Event& event)
{
	uint32_t modifiers = 0;
	
	if (event.key.mod & SDL_KMOD_SHIFT)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (event.key.mod & SDL_KMOD_CTRL)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (event.key.mod & SDL_KMOD_ALT)
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (event.key.mod & SDL_KMOD_GUI)
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	if (event.key.mod & SDL_KMOD_NUM)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (event.key.mod & SDL_KMOD_CAPS)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	
	return modifiers;
}

int CEF_Drawer::GetWindowsKeyCode(SDL_Scancode scancode, SDL_Keycode key)
{
	// Map SDL scancodes to Windows virtual key codes
	// This is a simplified mapping - add more as needed
	switch (scancode)
	{
		case SDL_SCANCODE_BACKSPACE: return 0x08; // VK_BACK
		case SDL_SCANCODE_TAB: return 0x09; // VK_TAB
		case SDL_SCANCODE_RETURN: return 0x0D; // VK_RETURN
		case SDL_SCANCODE_ESCAPE: return 0x1B; // VK_ESCAPE
		case SDL_SCANCODE_SPACE: return 0x20; // VK_SPACE
		case SDL_SCANCODE_DELETE: return 0x2E; // VK_DELETE
		case SDL_SCANCODE_LEFT: return 0x25; // VK_LEFT
		case SDL_SCANCODE_UP: return 0x26; // VK_UP
		case SDL_SCANCODE_RIGHT: return 0x27; // VK_RIGHT
		case SDL_SCANCODE_DOWN: return 0x28; // VK_DOWN
		case SDL_SCANCODE_HOME: return 0x24; // VK_HOME
		case SDL_SCANCODE_END: return 0x23; // VK_END
		case SDL_SCANCODE_PAGEUP: return 0x21; // VK_PRIOR
		case SDL_SCANCODE_PAGEDOWN: return 0x22; // VK_NEXT
		case SDL_SCANCODE_LSHIFT: return 0xA0; // VK_LSHIFT
		case SDL_SCANCODE_RSHIFT: return 0xA1; // VK_RSHIFT
		case SDL_SCANCODE_LCTRL: return 0xA2; // VK_LCONTROL
		case SDL_SCANCODE_RCTRL: return 0xA3; // VK_RCONTROL
		case SDL_SCANCODE_LALT: return 0xA4; // VK_LMENU
		case SDL_SCANCODE_RALT: return 0xA5; // VK_RMENU
		
		// F keys
		case SDL_SCANCODE_F1: return 0x70; // VK_F1
		case SDL_SCANCODE_F2: return 0x71;
		case SDL_SCANCODE_F3: return 0x72;
		case SDL_SCANCODE_F4: return 0x73;
		case SDL_SCANCODE_F5: return 0x74;
		case SDL_SCANCODE_F6: return 0x75;
		case SDL_SCANCODE_F7: return 0x76;
		case SDL_SCANCODE_F8: return 0x77;
		case SDL_SCANCODE_F9: return 0x78;
		case SDL_SCANCODE_F10: return 0x79;
		case SDL_SCANCODE_F11: return 0x7A;
		case SDL_SCANCODE_F12: return 0x7B;
		
		default:
			// For alphanumeric keys, use the SDL keycode
			if (key >= 32 && key < 127)
			{
				return toupper(key);
			}
			return 0;
	}
}

void CEF_Drawer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	static int last_width = -1;
	static int last_height = -1;
	
	rect.x = 0;
	rect.y = 0;
	rect.width = width_;
	rect.height = height_;
	
	if (width_ != last_width || height_ != last_height)
	{
		std::cout << "[CEF_Drawer] GetViewRect size changed: " << last_width << "x" << last_height << " -> " << width_ << "x" << height_ << std::endl;
		last_width = width_;
		last_height = height_;
	}
	else
	{
		static int call_count = 0;
		if (call_count++ % 120 == 0)
		{
			std::cout << "[CEF_Drawer] GetViewRect called: " << width_ << "x" << height_ << std::endl;
		}
	}
}

void CEF_Drawer::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
const RectList& dirtyRects, const void* buffer, int width, int height)
{
	if (type != PET_VIEW)
		return;
	
	initial_paint_count_++;
	
	if (is_initialized_render_complete_ && suppress_cef_repaints_ && !force_single_repaint_)
	{
		return;
	}
	
	if (force_single_repaint_)
	{
		force_single_repaint_ = false;
	}
	
	bool isInitialRendering = initial_paint_count_ <= 3;
	
	if (initial_paint_count_ == 1)
	{
		std::cout << "[CEF_Drawer] First OnPaint - Initial rendering started" << std::endl;
	}
	
	if (!isInitialRendering && initial_paint_count_ % 120 == 0)
	{
		std::cout << "[CEF_Drawer] OnPaint called: " << width << "x" << height 
		          << " buffer=" << (buffer ? "valid" : "null") 
		          << " dirtyRects=" << dirtyRects.size() 
		          << " (count=" << initial_paint_count_ << ")" << std::endl;
	}
	
	bool needsSecondInvalidate = false;
	CefRefPtr<CefBrowser> browserRef;
	
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		
		if (!has_received_first_paint_)
		{
			has_received_first_paint_ = true;
			std::cout << "[CEF_Drawer] First OnPaint received - rendering enabled" << std::endl;
		}
		
		if (width != width_ || height != height_)
		{
			std::cout << "[CEF_Drawer::OnPaint] CEF buffer size changed: " << width_ << "x" << height_ << " -> " << width << "x" << height << std::endl;
			width_ = width;
			paint_buffer_synchronized_ = false;
			resizer_->ensureTextureStorage(width_, height_);
		}
		
		if (width == logical_width_ && height == logical_height_)
		{
			if (!paint_buffer_synchronized_)
			{
				paint_buffer_synchronized_ = true;
				std::cout << "[CEF_Drawer::On Paint] Paint buffer synchronized with window size: " << width << "x" << height << std::endl;
				
				if (preserve_textures_during_resize_)
				{
					preserve_textures_during_resize_ = false;
					resize_wait_frames_ = 0;
					std::cout << "[CEF_Drawer::OnPaint] Resize complete - buffer synchronized, marking all textures as dirty for rebuild" << std::endl;
					
					for (auto& texturePair : ui_panel_textures_)
					{
						texturePair.second.dirty = true;
					}
				}
			}
		}
		else
		{
			if (paint_buffer_synchronized_)
			{
				paint_buffer_synchronized_ = false;
				std::cout << "[CEF_Drawer::OnPaint] Paint buffer desynchronized: buffer=" << width << "x" << height << " window=" << logical_width_ << "x" << logical_height_ << std::endl;
			}
			resizer_->ensureTextureStorage(width_, height_);
		}

		std::size_t bufferSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
		
		const int MIN_BUFFER_DIMENSION = 100;
		if (width < MIN_BUFFER_DIMENSION || height < MIN_BUFFER_DIMENSION)
		{
			std::cout << "[CEF_Drawer::OnPaint] Ignoring paint with too small buffer: " << width << "x" << height << " (min=" << MIN_BUFFER_DIMENSION << ")" << std::endl;
			return;
		}
		
		paint_buffer_width_ = width;
		paint_buffer_height_ = height;
		paint_buffer_.resize(bufferSize);
		if (buffer && bufferSize > 0)
		{
			std::memcpy(paint_buffer_.data(), buffer, bufferSize);
		}

		if (runtime_layout_waiting_for_paint_ && !runtime_layout_frames_.empty())
		{
			for (const auto& pair : runtime_layout_frames_)
			{
				if (pair.first != "viewport_panel")
				{
					std::cout << "  " << pair.first << ": " << pair.second.width << "x" << pair.second.height 
					          << " at (" << pair.second.x << "," << pair.second.y << ")" << std::endl;
					ui_panel_frames_[pair.first] = pair.second;
					ui_panel_display_frames_[pair.first] = pair.second;
				}
			}
			needsSecondInvalidate = runtime_layout_needs_second_invalidate_;
			runtime_layout_needs_second_invalidate_ = false;
		}

		runtime_layout_waiting_for_paint_ = false;

		if (isInitialRendering)
		{
			for (auto& texturePair : ui_panel_textures_)
			{
				texturePair.second.dirty = false;
			}

			bool markedDirtyPanel = false;
			for (auto& texturePair : ui_panel_textures_)
			{
				auto sourceIt = ui_panel_frames_.find(texturePair.first);
				if (sourceIt == ui_panel_frames_.end())
				{
					continue;
				}

				const UIPanelFrameData& sourceFrame = sourceIt->second;
				if (sourceFrame.width <= 0 || sourceFrame.height <= 0)
				{
					continue;
				}

				for (const CefRect& dirtyRect : dirtyRects)
				{
					const int dirtyMinX = (std::max)(dirtyRect.x, sourceFrame.x);
					const int dirtyMinY = (std::max)(dirtyRect.y, sourceFrame.y);
					const int dirtyMaxX = (std::min)(dirtyRect.x + dirtyRect.width, sourceFrame.x + sourceFrame.width);
					const int dirtyMaxY = (std::min)(dirtyRect.y + dirtyRect.height, sourceFrame.y + sourceFrame.height);

					if (dirtyMinX < dirtyMaxX && dirtyMinY < dirtyMaxY)
					{
						texturePair.second.dirty = true;
						markedDirtyPanel = true;
						break;
					}
				}
			}

			if (!markedDirtyPanel && dirtyRects.empty())
			{
				for (auto& texturePair : ui_panel_textures_)
				{
					texturePair.second.dirty = true;
				}
			}

			updateTexture(buffer, width, height);
		}
		
		if (initial_paint_count_ == 3)
		{
			is_initialized_render_complete_ = true;
			suppress_cef_repaints_ = true;
			std::cout << "[CEF_Drawer] Initial rendering complete after 3 paints - suppressing further repaints" << std::endl;
		}
		
		browserRef = browser_;
	}


	if (needsSecondInvalidate && browserRef)
	{
		std::cout << "[CEF_Drawer::OnPaint] Scheduling second invalidation to refresh interactive zones" << std::endl;
		CefPostTask(TID_UI, base::BindOnce([](CefRefPtr<CefBrowser> browser) {
			if (browser)
			{
				CefRefPtr<CefBrowserHost> host = browser->GetHost();
				if (host)
				{
					host->WasResized();
					host->Invalidate(PET_VIEW);
				}
			}
		}, browserRef));
	}
}
void CEF_Drawer::updateWindowProperties()
{
	if (!window_)
	{
		logical_width_ = 0;
		logical_height_ = 0;
		drawable_width_ = 0;
		drawable_height_ = 0;
		dpi_scale_ = 1.0f;
		return;
	}

	SDL_GetWindowSize(window_, &logical_width_, &logical_height_);
	SDL_GetWindowSizeInPixels(window_, &drawable_width_, &drawable_height_);

	if (logical_width_ > 0 && logical_height_ > 0 && drawable_width_ > 0 && drawable_height_ > 0)
	{
		dpi_scale_ = static_cast<float>(drawable_width_) / static_cast<float>(logical_width_);
	}
	else
	{
		dpi_scale_ = 1.0f;
	}
}

bool CEF_Drawer::createVulkanShaders()
{
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
	std::cout << "[CEF_Drawer::createVulkanShaders] Compiling vertex shader..." << std::endl;
	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[CEF_Drawer::createVulkanShaders] Failed to compile vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Compile fragment shader
	std::cout << "[CEF_Drawer::createVulkanShaders] Compiling fragment shader..." << std::endl;
	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[CEF_Drawer::createVulkanShaders] Failed to compile fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Create vertex shader module
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = vertexSPIRV.size() * sizeof(uint32_t);
	createInfo.pCode = vertexSPIRV.data();
	
	if (vkCreateShaderModule(vk_context_->getDevice(), &createInfo, nullptr, &vertex_shader_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer::createVulkanShaders] Failed to create vertex shader module" << std::endl;
		return false;
	}
	
	// Create fragment shader module
	createInfo.codeSize = fragmentSPIRV.size() * sizeof(uint32_t);
	createInfo.pCode = fragmentSPIRV.data();
	
	if (vkCreateShaderModule(vk_context_->getDevice(), &createInfo, nullptr, &fragment_shader_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer::createVulkanShaders] Failed to create fragment shader module" << std::endl;
		return false;
	}
	
	std::cout << "[CEF_Drawer::createVulkanShaders] Vulkan shaders created successfully" << std::endl;
	return true;
}

bool CEF_Drawer::createVertexBuffer()
{
	struct Vertex
	{
		float pos[2];
		float texCoord[2];
	};

	std::array<Vertex, 6> vertices = {{
		{{-1.0f, -1.0f}, {0.0f, 0.0f}},
		{{-1.0f,  1.0f}, {0.0f, 1.0f}},
		{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
		{{-1.0f, -1.0f}, {0.0f, 0.0f}},
		{{ 1.0f,  1.0f}, {1.0f, 1.0f}},
		{{ 1.0f, -1.0f}, {1.0f, 0.0f}}
	}};

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(vertices);
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(vk_context_->getDevice(), &bufferInfo, nullptr, &vertex_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to create vertex buffer" << std::endl;
		return false;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(vk_context_->getDevice(), vertex_buffer_, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(vk_context_->getDevice(), &allocInfo, nullptr, &vertex_buffer_memory_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to allocate vertex buffer memory" << std::endl;
		return false;
	}

	vkBindBufferMemory(vk_context_->getDevice(), vertex_buffer_, vertex_buffer_memory_, 0);

	void* data;
	vkMapMemory(vk_context_->getDevice(), vertex_buffer_memory_, 0, bufferInfo.size, 0, &data);
	memcpy(data, vertices.data(), bufferInfo.size);
	vkUnmapMemory(vk_context_->getDevice(), vertex_buffer_memory_);

	return true;
}

bool CEF_Drawer::createVulkanPipeline()
{
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
	std::cout << "[CEF_Drawer] Compiling vertex shader..." << std::endl;
	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[CEF_Drawer] Failed to compile vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Compile fragment shader
	std::cout << "[CEF_Drawer] Compiling fragment shader..." << std::endl;
	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[CEF_Drawer] Failed to compile fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	// Convert SPIR-V to byte vectors
	std::vector<char> vertShaderBytes = GLSLCompiler::spirvToBytes(vertexSPIRV);
	std::vector<char> fragShaderBytes = GLSLCompiler::spirvToBytes(fragmentSPIRV);

	VkDevice device = vk_context_->getDevice();

	// Create descriptor set layout for texture sampler
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = 0;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layoutBinding.pImmutableSamplers = nullptr;
	
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &layoutBinding;
	
	if (vkCreateDescriptorSetLayout(vk_context_->getDevice(), &layoutInfo, nullptr, &descriptor_set_layout_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to create descriptor set layout" << std::endl;
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
	
	if (vkCreateDescriptorPool(vk_context_->getDevice(), &poolInfo, nullptr, &descriptor_pool_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to create descriptor pool" << std::endl;
		return false;
	}
	
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptor_pool_;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptor_set_layout_;
	
	if (vkAllocateDescriptorSets(vk_context_->getDevice(), &allocInfo, &descriptor_set_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to allocate descriptor set" << std::endl;
		return false;
	}
	
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = texture_view_;
	imageInfo.sampler = texture_sampler_;
	
	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptor_set_;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;
	
	vkUpdateDescriptorSets(vk_context_->getDevice(), 1, &descriptorWrite, 0, nullptr);

	// Configure vertex attributes
	VkVertexInputAttributeDescription attr0{};
	attr0.binding = 0;
	attr0.location = 0;
	attr0.format = VK_FORMAT_R32G32_SFLOAT;  // vec2 position
	attr0.offset = 0;

	VkVertexInputAttributeDescription attr1{};
	attr1.binding = 0;
	attr1.location = 1;
	attr1.format = VK_FORMAT_R32G32_SFLOAT;  // vec2 texCoord
	attr1.offset = 2 * sizeof(float);

	std::vector<VkVertexInputAttributeDescription> vertexAttributes = { attr0, attr1 };

	// Build pipeline configuration for VulkanPipeline factory
	VulkanPipeline::PipelineConfig config;
	config.name = "cef_drawer_texture";
	config.vertexShaderCode = vertShaderBytes;
	config.fragmentShaderCode = fragShaderBytes;
	config.renderPass = vk_render_pass_;
	config.descriptorSetLayout = descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 4 * sizeof(float);  // vec2 pos + vec2 texCoord = 16 bytes
	config.vertexAttributes = vertexAttributes;

	std::cout << "[CEF_Drawer] ========== Requesting pipeline from VulkanPipeline factory..." << std::endl;
	std::cout << "[CEF_Drawer] Pipeline name: " << config.name << std::endl;
	std::cout << "[CEF_Drawer] Vertex shader size: " << config.vertexShaderCode.size() << " bytes" << std::endl;
	std::cout << "[CEF_Drawer] Fragment shader size: " << config.fragmentShaderCode.size() << " bytes" << std::endl;
	std::cout << "[CEF_Drawer] RenderPass: " << config.renderPass << std::endl;
	std::cout << "[CEF_Drawer] DescriptorSetLayout: " << config.descriptorSetLayout << std::endl;
	std::cout << "[CEF_Drawer] Topology: " << config.topology << std::endl;
	std::cout << "[CEF_Drawer] Vertex stride: " << config.vertexBindingStride << " bytes" << std::endl;
	std::cout << "[CEF_Drawer] Vertex attributes: " << config.vertexAttributes.size() << std::endl;

	// Get or create pipeline from factory
	shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);

	if (!shared_pipeline_)
	{
		std::cerr << "[CEF_Drawer] Failed to get/create pipeline from VulkanPipeline factory" << std::endl;
		return false;
	}

	if (shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[CEF_Drawer] ERROR: Pipeline created but handle is NULL!" << std::endl;
		return false;
	}

	// Store handles for compatibility (deprecated, but kept for now)
	pipeline_ = shared_pipeline_->pipeline;
	pipeline_layout_ = shared_pipeline_->layout;
	vertex_shader_ = shared_pipeline_->vertexShader;
	fragment_shader_ = shared_pipeline_->fragmentShader;

	std::cout << "[CEF_Drawer] Vulkan pipeline acquired successfully from factory!" << std::endl;
	std::cout << "[CEF_Drawer] Pipeline handle: " << shared_pipeline_->pipeline << std::endl;
	std::cout << "[CEF_Drawer] Pipeline layout: " << shared_pipeline_->layout << std::endl;
	std::cout << "[CEF_Drawer] Vertex shader: " << shared_pipeline_->vertexShader << std::endl;
	std::cout << "[CEF_Drawer] Fragment shader: " << shared_pipeline_->fragmentShader << std::endl;
	std::cout << "========================================" << std::endl;

	return true;
}

bool CEF_Drawer::createVulkanResources()
{
	std::cout << "[CEF_Drawer::createVulkanResources] Starting..." << std::endl;
	
	// Shader creation now handled by VulkanPipeline factory in createVulkanPipeline()
	// No need to call createVulkanShaders() anymore
	std::cout << "[CEF_Drawer::createVulkanResources] Shaders will be created by VulkanPipeline factory" << std::endl;

	if (!createVertexBuffer())
	{
		std::cerr << "[CEF_Drawer::createVulkanResources] Failed to create vertex buffer" << std::endl;
		return false;
	}
	std::cout << "[CEF_Drawer::createVulkanResources] Vertex buffer created" << std::endl;

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width_;
	imageInfo.extent.height = height_;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(vk_context_->getDevice(), &imageInfo, nullptr, &texture_image_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to create texture image" << std::endl;
		return false;
	}
	std::cout << "[CEF_Drawer::createVulkanResources] Texture image created" << std::endl;

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vk_context_->getDevice(), texture_image_, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(vk_context_->getDevice(), &allocInfo, nullptr, &texture_memory_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to allocate texture memory" << std::endl;
		return false;
	}
	std::cout << "[CEF_Drawer::createVulkanResources] Texture memory allocated" << std::endl;

	vkBindImageMemory(vk_context_->getDevice(), texture_image_, texture_memory_, 0);
	std::cout << "[CEF_Drawer::createVulkanResources] Texture image bound to memory" << std::endl;

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = texture_image_;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(vk_context_->getDevice(), &viewInfo, nullptr, &texture_view_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to create texture image view" << std::endl;
		return false;
	}
	std::cout << "[CEF_Drawer::createVulkanResources] Texture image view created" << std::endl;

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(vk_context_->getDevice(), &samplerInfo, nullptr, &texture_sampler_) != VK_SUCCESS)
	{
		std::cerr << "[CEF_Drawer] Failed to create texture sampler" << std::endl;
		return false;
	}
	std::cout << "[CEF_Drawer::createVulkanResources] Texture sampler created" << std::endl;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = 0;
	
	if (vkCreateFence(vk_context_->getDevice(), &fenceInfo, nullptr, &fence) != VK_SUCCESS)
	{
		texture_layout_initialized_ = true;
		descriptor_needs_update_ = true;
		std::cout << "[CEF_Drawer] Vulkan resources created (pipeline will be created after render pass is set)" << std::endl;
		return true;
	}
	
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = 0;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	
	if (vkCreateCommandPool(vk_context_->getDevice(), &poolInfo, nullptr, &commandPool) == VK_SUCCESS)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;
		
		if (vkAllocateCommandBuffers(vk_context_->getDevice(), &allocInfo, &commandBuffer) == VK_SUCCESS)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			
			vkBeginCommandBuffer(commandBuffer, &beginInfo);
			
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = texture_image_;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			
			vkCmdPipelineBarrier(commandBuffer, 
				VK_PIPELINE_STAGE_HOST_BIT, 
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);
			
			vkEndCommandBuffer(commandBuffer);
			
			VkSubmitInfo submitInfo{};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffer;
			
			vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, fence);
			vkWaitForFences(vk_context_->getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
			
			vkFreeCommandBuffers(vk_context_->getDevice(), commandPool, 1, &commandBuffer);
		}
		
		vkDestroyCommandPool(vk_context_->getDevice(), commandPool, nullptr);
	}
	
	if (fence != VK_NULL_HANDLE)
	{
		vkDestroyFence(vk_context_->getDevice(), fence, nullptr);
	}
	
	texture_layout_initialized_ = true;
	descriptor_needs_update_ = true;
	
	std::cout << "[CEF_Drawer] Vulkan resources created (pipeline will be created after render pass is set)" << std::endl;
	return true;
}

void CEF_Drawer::cleanupVulkanResources()
{
	if (!vk_context_)
	{
		return;
	}

	VkDevice device = vk_context_->getDevice();
	
	vkDeviceWaitIdle(device);

	if (texture_sampler_ != VK_NULL_HANDLE)
	{
		vkDestroySampler(device, texture_sampler_, nullptr);
		texture_sampler_ = VK_NULL_HANDLE;
	}

	if (texture_view_ != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, texture_view_, nullptr);
		texture_view_ = VK_NULL_HANDLE;
	}

	if (texture_image_ != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, texture_image_, nullptr);
		texture_image_ = VK_NULL_HANDLE;
	}

	if (texture_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, texture_memory_, nullptr);
		texture_memory_ = VK_NULL_HANDLE;
	}

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

	if (descriptor_pool_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, descriptor_pool_, nullptr);
		descriptor_pool_ = VK_NULL_HANDLE;
	}

	if (descriptor_set_layout_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device, descriptor_set_layout_, nullptr);
		descriptor_set_layout_ = VK_NULL_HANDLE;
	}

	// Pipeline resources now managed by VulkanPipeline factory via shared_pipeline_
	// Release the shared_ptr to decrement reference count
	if (shared_pipeline_)
	{
		std::cout << "[CEF_Drawer] Releasing shared pipeline reference" << std::endl;
		shared_pipeline_.reset();
	}
	
	// Clear deprecated handles (no manual destruction needed)
	pipeline_ = VK_NULL_HANDLE;
	pipeline_layout_ = VK_NULL_HANDLE;
	vertex_shader_ = VK_NULL_HANDLE;
	fragment_shader_ = VK_NULL_HANDLE;
	
	texture_layout_initialized_ = false;
}

uint32_t CEF_Drawer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

	std::cerr << "[CEF_Drawer] Failed to find suitable memory type" << std::endl;
	return 0;
}

void CEF_Drawer::updateTexture(const void* buffer, int width, int height)
{
	if (!buffer || texture_image_ == VK_NULL_HANDLE)
		return;

	if (!texture_layout_initialized_)
	{
		VkCommandPool commandPool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
		
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = 0;
		
		if (vkCreateFence(vk_context_->getDevice(), &fenceInfo, nullptr, &fence) != VK_SUCCESS)
		{
			texture_layout_initialized_ = true;
			descriptor_needs_update_ = true;
			return;
		}
		
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = 0;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		
		if (vkCreateCommandPool(vk_context_->getDevice(), &poolInfo, nullptr, &commandPool) == VK_SUCCESS)
		{
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = commandPool;
			allocInfo.commandBufferCount = 1;
			
			if (vkAllocateCommandBuffers(vk_context_->getDevice(), &allocInfo, &commandBuffer) == VK_SUCCESS)
			{
				VkCommandBufferBeginInfo beginInfo{};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				
				vkBeginCommandBuffer(commandBuffer, &beginInfo);
				
				VkImageMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
				barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barrier.image = texture_image_;
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barrier.subresourceRange.baseMipLevel = 0;
				barrier.subresourceRange.levelCount = 1;
				barrier.subresourceRange.baseArrayLayer = 0;
				barrier.subresourceRange.layerCount = 1;
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				
				vkCmdPipelineBarrier(commandBuffer, 
					VK_PIPELINE_STAGE_HOST_BIT, 
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0, 0, nullptr, 0, nullptr, 1, &barrier);
				
				vkEndCommandBuffer(commandBuffer);
				
				VkSubmitInfo submitInfo{};
				submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;
				
				vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, fence);
				vkWaitForFences(vk_context_->getDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
				
				vkFreeCommandBuffers(vk_context_->getDevice(), commandPool, 1, &commandBuffer);
			}
			
			vkDestroyCommandPool(vk_context_->getDevice(), commandPool, nullptr);
		}
		
		if (fence != VK_NULL_HANDLE)
		{
			vkDestroyFence(vk_context_->getDevice(), fence, nullptr);
		}
		
		texture_layout_initialized_ = true;
		descriptor_needs_update_ = true;
		std::cout << "[CEF_Drawer] Texture layout transitioned to GENERAL" << std::endl;
	}

	VkImageSubresource subresource{};
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(vk_context_->getDevice(), texture_image_, &subresource, &layout);

	void* data;
	vkMapMemory(vk_context_->getDevice(), texture_memory_, 0, VK_WHOLE_SIZE, 0, &data);

	if (layout.rowPitch == width * 4)
	{
		memcpy(data, buffer, width * height * 4);
	}
	else
	{
		uint8_t* dataBytes = reinterpret_cast<uint8_t*>(data);
		const uint8_t* bufferBytes = reinterpret_cast<const uint8_t*>(buffer);
		for (int y = 0; y < height; y++)
		{
			memcpy(dataBytes + (y * layout.rowPitch), bufferBytes + (y * width * 4), width * 4);
		}
	}

	vkUnmapMemory(vk_context_->getDevice(), texture_memory_);
	descriptor_needs_update_ = true;
}