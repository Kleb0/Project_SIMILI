#include "SDL_ApplicationWindow.hpp"
// #include "ui_handler.hpp"
#include "ResourcesLoader.hpp"
#include "viewportLogic/UIPanels/FrameDataCatcher.hpp"
#include "viewportLogic/FrameDatas/FrameDatas.hpp"
#include "viewportLogic/UIPanels/PanelResizingLogic.hpp"
// #include "viewportLogic/ThreeDScreen/ThreeDScreen.hpp"
#include "viewportLogic/UIPanels/UIManager.hpp"
#include "viewportLogic/UIPanels/Splitter.hpp"
#include "App_Border.hpp"
#include "Frontend_Debug_Tools/Enable_UI_Debug_Tools.hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include "../../Engine/VulkanScene/VKScene.Hpp"
#include "../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "viewportLogic/Raycasting/RaycastPerform.hpp"
#include <SDL3/SDL_vulkan.h>
#include "include/cef_browser.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include <set>
#include <cstring>

// Static member definition
int SDL_ApplicationWindow::render_frame_count = 0;

// ======== SIMILICefClient ========

SIMILICefClient::SIMILICefClient(SDL_ApplicationWindow* owner)
	: owner_(owner)
	, resource_request_handler_(nullptr)
{
}

CefRefPtr<CefRenderHandler> SIMILICefClient::GetRenderHandler()
{
	if (owner_)
		return owner_->getRenderHandler();
	return nullptr;
}

CefRefPtr<CefResourceRequestHandler> SIMILICefClient::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling)
{
	if (!resource_request_handler_)
	{
		resource_request_handler_ = ResourcesLoader::createRequestHandler();
	}
	return resource_request_handler_;
}

// ======== AppRenderHandler ========

AppRenderHandler::AppRenderHandler(SDL_ApplicationWindow* owner)
	: owner_(owner)
{
}

void AppRenderHandler::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	int w = 1920, h = 1080;
	if (owner_ && owner_->getHandle())
		SDL_GetWindowSize(owner_->getHandle(), &w, &h);
	rect = CefRect(0, 0, w, h);
}

void AppRenderHandler::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
	const RectList& dirtyRects, const void* buffer, int width, int height)
{
	if (owner_)
		owner_->onCEFPaint(type, buffer, width, height);
}

SDL_ApplicationWindow::SDL_ApplicationWindow()
	: window_(nullptr)
	, is_maximized_(false)
	, last_x_(0)
	, last_y_(0)
	, last_width_(800)
	, last_height_(600)
	, dpi_scale_(1.0f)
	, reference_window_width_(1920)
	, reference_window_height_(1080)
	, ui_handler_(nullptr)
	, threed_screen_(nullptr)
	, vk_scene_(nullptr)
	, camera_(nullptr)
	, camera_control_(nullptr)
	, frame_datas_(nullptr)
	, ui_manager_(nullptr)
	, panel_resizing_logic_(nullptr)
	, app_border_(nullptr)
	, debug_tools_(nullptr)
	, selector_(nullptr)
	, raycast_perform_(nullptr)
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
	, frame_acquisition_succeeded_(false)
	, prepared_drawable_width_(0)
	, prepared_drawable_height_(0)
	, prepared_skip_texture_rebuild_(false)
	, borders_set_for_init_(false)
	, pending_window_resize_sync_(false)
	, pending_wheel_delta_(0.0f)
	, prev_middle_button_down_(false)
	, prev_left_button_down_(false)
	, pending_left_click_(false)
	, browser_(nullptr)
	, simili_cef_client_(nullptr)
	, ui_manager_vulkan_initialized_(false)
	, cef_render_handler_(new AppRenderHandler(this))
	, cef_shared_pipeline_(nullptr)
	, cef_descriptor_set_layout_(VK_NULL_HANDLE)
	, cef_paint_width_(0)
	, cef_paint_height_(0)
	, cef_texture_image_(VK_NULL_HANDLE)
	, cef_texture_memory_(VK_NULL_HANDLE)
	, cef_texture_view_(VK_NULL_HANDLE)
	, cef_texture_sampler_(VK_NULL_HANDLE)
	, cef_texture_uploaded_width_(0)
	, cef_texture_uploaded_height_(0)
	, vk_depth_image_(VK_NULL_HANDLE)
	, vk_depth_image_memory_(VK_NULL_HANDLE)
	, vk_depth_image_view_(VK_NULL_HANDLE)
	, state_init_(this)
	, state_maximized_(this)
	, state_reduced_(this)
	, state_scaling_down_(this)
	, state_scaling_up_(this)
	, current_state_(&state_init_)
	, mouse_state_above_workspace_()
	, mouse_state_outside_workspace_()
	, mouse_state_above_ui_panel_()
	, current_mouse_state_(&mouse_state_outside_workspace_)
	, imgui_descriptor_pool_(VK_NULL_HANDLE)
	, imgui_render_pass_(VK_NULL_HANDLE)
	, imgui_command_buffer_(VK_NULL_HANDLE)
	, imgui_initialized_(false)
	, workspace_widget_(nullptr)
	, widget_command_buffer_(VK_NULL_HANDLE)
	, contextual_menu_above_gui_(nullptr)
	, contextual_menu_command_buffer_(VK_NULL_HANDLE)
	, contextual_menu_visible_(false)
{
}
SDL_ApplicationWindow::~SDL_ApplicationWindow()
{
	if (workspace_widget_)
	{
		workspace_widget_->shutdown();
		delete workspace_widget_;
		workspace_widget_ = nullptr;
	}
	if (contextual_menu_above_gui_)
	{
		contextual_menu_above_gui_->shutdown();
		delete contextual_menu_above_gui_;
		contextual_menu_above_gui_ = nullptr;
	}
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
	
	// Set SDL reference window size (used for JavaScript scaling)
	reference_window_width_ = width;
	reference_window_height_ = height;
	std::cout << "[SDL_ApplicationWindow] Reference window size set to: " << reference_window_width_ << "x" << reference_window_height_ << std::endl;
	
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
	app_border_->updateDimensions(width, height);
	
	std::cout << "[SDL_ApplicationWindow] App_Border initialized" << std::endl;
	
	if (!debug_tools_)
	{
		debug_tools_ = new Enable_UI_Debug_Tools();
	}

	if (current_state_)
		current_state_->enter_state();

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

WindowRenderState SDL_ApplicationWindow::getWindowRenderState() const
{
	if (current_state_ == &state_maximized_) return WindowRenderState::Maximized;
	if (current_state_ == &state_reduced_) return WindowRenderState::Reduced;
	if (current_state_ == &state_scaling_up_) return WindowRenderState::ScaleUp;
	if (current_state_ == &state_scaling_down_) return WindowRenderState::ScaleDown;
	return WindowRenderState::Init;
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

void SDL_ApplicationWindow::onCEFPaint(CefRenderHandler::PaintElementType type, const void* buffer, int width, int height)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
	cef_paint_buffer_.resize(size);
	if (buffer && size > 0)
		std::memcpy(cef_paint_buffer_.data(), buffer, size);
	cef_paint_width_ = width;
	cef_paint_height_ = height;
}

void SDL_ApplicationWindow::setRenderPass(VkRenderPass renderPass)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	std::cout << "[SDL_ApplicationWindow] setRenderPass called with renderPass=" << renderPass << std::endl;

	if (renderPass == VK_NULL_HANDLE)
	{
		std::cerr << "[SDL_ApplicationWindow] ERROR: Render pass is VK_NULL_HANDLE!" << std::endl;
		return;
	}

	vk_render_pass_ = renderPass;
	std::cout << "[SDL_ApplicationWindow] Render pass set: " << renderPass << std::endl;

	if (cef_shared_pipeline_)
	{
		std::cout << "[SDL_ApplicationWindow] Releasing old CEF pipeline (shared_ptr)" << std::endl;
		cef_shared_pipeline_.reset();
	}

	if (!vulkan_pipelines_ || !vulkan_pipelines_->isInitialized())
	{
		std::cerr << "[SDL_ApplicationWindow] ERROR: VulkanPipeline system is not initialized" << std::endl;
		return;
	}

	if (!createCEFPipeline())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create CEF Vulkan pipeline with render pass" << std::endl;
	}
	else
	{
		std::cout << "[SDL_ApplicationWindow] CEF pipeline created successfully, handle="
			<< (cef_shared_pipeline_ ? cef_shared_pipeline_->pipeline : VK_NULL_HANDLE) << std::endl;
	}

	if (imgui_render_pass_ != VK_NULL_HANDLE && !workspace_widget_)
	{
		workspace_widget_ = new SIMILI::Frontend::WorkspaceWidget();
		if (!workspace_widget_->initializeVulkan(vk_context_, vulkan_pipelines_, imgui_render_pass_, 400, 80))
		{
			std::cerr << "[SDL_ApplicationWindow] WorkspaceWidget::initializeVulkan failed" << std::endl;
			delete workspace_widget_;
			workspace_widget_ = nullptr;
		}
		else
		{
			std::cout << "[SDL_ApplicationWindow] WorkspaceWidget initialized" << std::endl;
		}
	}

	if (imgui_render_pass_ != VK_NULL_HANDLE && !contextual_menu_above_gui_)
	{
		int winW = 1920, winH = 1080;
		if (window_)
		{
			SDL_GetWindowSizeInPixels(window_, &winW, &winH);
		}
		int menuW = winW / 4;
		int menuH = winH / 2;
		contextual_menu_above_gui_ = new SIMILI::Frontend::ContextualMenuAboveGUI();
		if (!contextual_menu_above_gui_->initializeVulkan(vk_context_, vulkan_pipelines_, imgui_render_pass_, menuW, menuH))
		{
			std::cerr << "[SDL_ApplicationWindow] ContextualMenuAboveGUI::initializeVulkan failed" << std::endl;
			delete contextual_menu_above_gui_;
			contextual_menu_above_gui_ = nullptr;
		}
		else
		{
			contextual_menu_above_gui_->SetAlpha(85.0f);
			std::cout << "[SDL_ApplicationWindow] ContextualMenuAboveGUI initialized with size " << menuW << "x" << menuH << " and alpha 50%" << std::endl;
		}
	}
}

bool SDL_ApplicationWindow::createCEFPipeline()
{
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

		const std::string fragmentShaderGLSL = R"(
	#version 450

	layout(location = 0) in vec2 vTexCoord;
	layout(location = 0) out vec4 outColor;

	layout(binding = 0) uniform sampler2D texSampler;

	void main() {
		outColor = texture(texSampler, vTexCoord);
	}
	)";

	std::vector<uint32_t> vertexSPIRV = GLSLCompiler::compileGLSL(vertexShaderGLSL, GLSLCompiler::ShaderType::Vertex);
	if (vertexSPIRV.empty())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to compile CEF vertex shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	std::vector<uint32_t> fragmentSPIRV = GLSLCompiler::compileGLSL(fragmentShaderGLSL, GLSLCompiler::ShaderType::Fragment);
	if (fragmentSPIRV.empty())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to compile CEF fragment shader: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	std::vector<char> vertShaderBytes = GLSLCompiler::spirvToBytes(vertexSPIRV);
	std::vector<char> fragShaderBytes = GLSLCompiler::spirvToBytes(fragmentSPIRV);

	if (cef_descriptor_set_layout_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(vk_context_->getDevice(), cef_descriptor_set_layout_, nullptr);
		cef_descriptor_set_layout_ = VK_NULL_HANDLE;
	}

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

	if (vkCreateDescriptorSetLayout(vk_context_->getDevice(), &layoutInfo, nullptr, &cef_descriptor_set_layout_) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create CEF descriptor set layout" << std::endl;
		return false;
	}

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

	VulkanPipeline::PipelineConfig config;
	config.name = "cef_app_window_texture";
	config.vertexShaderCode = vertShaderBytes;
	config.fragmentShaderCode = fragShaderBytes;
	config.renderPass = vk_render_pass_;
	config.descriptorSetLayout = cef_descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 4 * sizeof(float);
	config.vertexAttributes = { attr0, attr1 };

	cef_shared_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);

	if (!cef_shared_pipeline_ || cef_shared_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to create CEF pipeline from factory" << std::endl;
		return false;
	}

	std::cout << "[SDL_ApplicationWindow] CEF pipeline acquired: " << cef_shared_pipeline_->pipeline << std::endl;
	return true;
}

void SDL_ApplicationWindow::uploadCEFPaintBuffer()
{
	std::vector<unsigned char> localBuffer;
	int w = 0, h = 0;
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		if (cef_paint_buffer_.empty() || cef_paint_width_ <= 0 || cef_paint_height_ <= 0)
			return;
		localBuffer = cef_paint_buffer_;
		w = cef_paint_width_;
		h = cef_paint_height_;
	}

	VkDevice device = vk_context_->getDevice();

	if (cef_texture_image_ != VK_NULL_HANDLE && (w != cef_texture_uploaded_width_ || h != cef_texture_uploaded_height_))
	{
		{
			std::lock_guard<std::mutex> qlock(queue_mutex_);
			vkQueueWaitIdle(vk_context_->getGraphicsQueue());
		}
		if (cef_texture_view_ != VK_NULL_HANDLE) { vkDestroyImageView(device, cef_texture_view_, nullptr); cef_texture_view_ = VK_NULL_HANDLE; }
		if (cef_texture_sampler_ != VK_NULL_HANDLE) { vkDestroySampler(device, cef_texture_sampler_, nullptr); cef_texture_sampler_ = VK_NULL_HANDLE; }
		if (cef_texture_image_ != VK_NULL_HANDLE) { vkDestroyImage(device, cef_texture_image_, nullptr); cef_texture_image_ = VK_NULL_HANDLE; }
		if (cef_texture_memory_ != VK_NULL_HANDLE) { vkFreeMemory(device, cef_texture_memory_, nullptr); cef_texture_memory_ = VK_NULL_HANDLE; }
		
		if (ui_manager_)
		{
			ui_manager_->invalidateCEFTexture();
		}
	}

	if (cef_texture_image_ == VK_NULL_HANDLE)
	{
		VkImageCreateInfo imgInfo{};
		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.extent = { (uint32_t)w, (uint32_t)h, 1 };
		imgInfo.mipLevels = 1; imgInfo.arrayLayers = 1;
		imgInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
		imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateImage(device, &imgInfo, nullptr, &cef_texture_image_) != VK_SUCCESS) return;

		VkMemoryRequirements memReq;
		vkGetImageMemoryRequirements(device, cef_texture_image_, &memReq);
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProps);
		uint32_t memIdx = 0;
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
		{
			if ((memReq.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			{ memIdx = i; break; }
		}
		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = memReq.size; allocInfo.memoryTypeIndex = memIdx;
		if (vkAllocateMemory(device, &allocInfo, nullptr, &cef_texture_memory_) != VK_SUCCESS) return;
		vkBindImageMemory(device, cef_texture_image_, cef_texture_memory_, 0);

		VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = cef_texture_image_; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		if (vkCreateImageView(device, &viewInfo, nullptr, &cef_texture_view_) != VK_SUCCESS) return;

		if (!createCEFTextureSampler(device)) return;

		cef_texture_uploaded_width_ = 0;
		cef_texture_uploaded_height_ = 0;
	}

	VkDeviceSize bufSize = (VkDeviceSize)w * h * 4;
	VkBufferCreateInfo stagingInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	stagingInfo.size = bufSize; stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	VkBuffer staging = VK_NULL_HANDLE;
	if (vkCreateBuffer(device, &stagingInfo, nullptr, &staging) != VK_SUCCESS) return;

	VkMemoryRequirements stagingReq;
	vkGetBufferMemoryRequirements(device, staging, &stagingReq);
	VkPhysicalDeviceMemoryProperties memProps2;
	vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProps2);
	uint32_t stagingIdx = 0;

	for (uint32_t i = 0; i < memProps2.memoryTypeCount; i++)
	{
		if ((stagingReq.memoryTypeBits & (1 << i)) && (memProps2.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)))
		{ stagingIdx = i; break; }
	}

	VkMemoryAllocateInfo stagingAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	stagingAlloc.allocationSize = stagingReq.size; stagingAlloc.memoryTypeIndex = stagingIdx;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;
	if (vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingMem) != VK_SUCCESS) { vkDestroyBuffer(device, staging, nullptr); return; }
	vkBindBufferMemory(device, staging, stagingMem, 0);
	void* mapped = nullptr;
	vkMapMemory(device, stagingMem, 0, bufSize, 0, &mapped);
	std::memcpy(mapped, localBuffer.data(), static_cast<size_t>(bufSize));
	vkUnmapMemory(device, stagingMem);

	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex = vk_context_->getGraphicsQueueFamily();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	VkCommandPool pool = VK_NULL_HANDLE;
	vkCreateCommandPool(device, &poolInfo, nullptr, &pool);
	VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmdAlloc.commandPool = pool; cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cmdAlloc.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);
	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	VkImageMemoryBarrier toTransfer{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	toTransfer.oldLayout = (cef_texture_uploaded_width_ == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toTransfer.srcQueueFamilyIndex = toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.image = cef_texture_image_;
	toTransfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	toTransfer.srcAccessMask = (cef_texture_uploaded_width_ == 0) ? 0 : VK_ACCESS_SHADER_READ_BIT;
	toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

	VkBufferImageCopy region{};
	region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	region.imageExtent = { (uint32_t)w, (uint32_t)h, 1 };
	vkCmdCopyBufferToImage(cmd, staging, cef_texture_image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	VkImageMemoryBarrier toGeneral{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	toGeneral.srcQueueFamilyIndex = toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.image = cef_texture_image_;
	toGeneral.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);

	vkEndCommandBuffer(cmd);
	VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
	{
		std::lock_guard<std::mutex> qlock(queue_mutex_);
		vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submit, VK_NULL_HANDLE);
		vkQueueWaitIdle(vk_context_->getGraphicsQueue());
	}
	vkDestroyCommandPool(device, pool, nullptr);
	vkFreeMemory(device, stagingMem, nullptr);
	vkDestroyBuffer(device, staging, nullptr);

	cef_texture_uploaded_width_ = w;
	cef_texture_uploaded_height_ = h;

	if (ui_manager_)
		ui_manager_->setCEFTextureForAllPanels(cef_texture_view_, cef_texture_sampler_, cef_texture_uploaded_width_, cef_texture_uploaded_height_);
}

bool SDL_ApplicationWindow::createCEFTextureSampler(VkDevice device)
{
		if (device == VK_NULL_HANDLE)
		{
			return false;
		}

		if (cef_texture_sampler_ != VK_NULL_HANDLE)
		{
			vkDestroySampler(device, cef_texture_sampler_, nullptr);
			cef_texture_sampler_ = VK_NULL_HANDLE;
		}

		VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		return vkCreateSampler(device, &samplerInfo, nullptr, &cef_texture_sampler_) == VK_SUCCESS;
	}

void SDL_ApplicationWindow::SetHTMLAdressToDraw(CefRefPtr<CefClient> client, const std::string& url, int width, int height)
{
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	browser_settings.javascript = STATE_ENABLED;
	browser_settings.javascript_close_windows = STATE_ENABLED;
	browser_settings.javascript_access_clipboard = STATE_ENABLED;
	browser_settings.javascript_dom_paste = STATE_ENABLED;

	CefWindowInfo window_info;
	window_info.SetAsWindowless(0);

	CefRefPtr<CefBrowser> browser = CefBrowserHost::CreateBrowserSync(window_info, client, url, browser_settings, nullptr, nullptr);
	if (!browser)
	{
		std::cerr << "[SDL_ApplicationWindow] SetHTMLAdressToDraw: échec création browser CEF" << std::endl;
		return;
	}

	std::cout << "[SDL_ApplicationWindow] Browser CEF créé : " << url << " (" << width << "x" << height << ")" << std::endl;

	browser_ = browser;
}

void SDL_ApplicationWindow::requestBrowserRepaint()
{
	if (!browser_ || !browser_->GetHost())
	{
		return;
	}

	browser_->GetHost()->WasHidden(false);
	browser_->GetHost()->NotifyScreenInfoChanged();
	browser_->GetHost()->WasResized();
	browser_->GetHost()->Invalidate(PET_VIEW);
	std::cout << "[SDL_ApplicationWindow] Requested CEF resize/repaint after splitter release" << std::endl;
}


// ====== Event Handling ====== //

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

		if (current_state_ != &state_scaling_up_ && current_state_ != &state_scaling_down_ && current_state_ != &state_maximized_)
		{
			StateTransition(current_state_, &state_reduced_);
		}

		if (!isMaximized() && current_state_ != &state_maximized_)
		{
			pending_window_resize_sync_ = true;
		}
	}
	
	bool currentMax = isMaximized();
	if (currentMax != is_maximized_)
	{
		is_maximized_ = currentMax;
		std::cout << "[SDL_ApplicationWindow] Window " << (is_maximized_ ? "MAXIMIZED" : "RESTORED") << std::endl;
		swapchain_needs_recreation_ = true;

		if (is_maximized_)
			StateTransition(current_state_, &state_scaling_up_);
		else
			StateTransition(current_state_, &state_scaling_down_);
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

void SDL_ApplicationWindow::handleSDLEvent(const SDL_Event& event)
{
	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT)
	{
		contextual_menu_visible_ = !contextual_menu_visible_;
		if (contextual_menu_visible_ && contextual_menu_above_gui_ && window_)
		{
			int logicalW = 0, logicalH = 0;
			SDL_GetWindowSize(window_, &logicalW, &logicalH);
			int drawableW = 0, drawableH = 0;
			SDL_GetWindowSizeInPixels(window_, &drawableW, &drawableH);
			float scaleX = (logicalW > 0) ? (static_cast<float>(drawableW) / static_cast<float>(logicalW)) : 1.0f;
			float scaleY = (logicalH > 0) ? (static_cast<float>(drawableH) / static_cast<float>(logicalH)) : 1.0f;

			int menuW = contextual_menu_above_gui_->getWidgetWidth();
			int menuH = contextual_menu_above_gui_->getWidgetHeight();

			float logicalMenuW = menuW / scaleX;
			float logicalMenuH = menuH / scaleY;

			int setX = static_cast<int>(event.button.x);
			int setY = static_cast<int>(event.button.y);

			if (setX + static_cast<int>(logicalMenuW) > logicalW) {
				setX = logicalW - static_cast<int>(logicalMenuW);
			}
			if (setY + static_cast<int>(logicalMenuH) > logicalH) {
				setY = logicalH - static_cast<int>(logicalMenuH);
			}
			if (setX < 0) setX = 0;
			if (setY < 0) setY = 0;

			contextual_menu_above_gui_->SetPos(setX, setY);
		}
	}

	if (event.type == SDL_EVENT_MOUSE_WHEEL && current_mouse_state_ == &mouse_state_above_workspace_)
	{
		pending_wheel_delta_ += event.wheel.y;
	}

	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT
		&& current_mouse_state_ == &mouse_state_above_workspace_)
	{
		pending_left_click_ = true;
	}

	// Forward physical keys 1-4 to the WorkspaceWidget CEF browser regardless of keyboard layout
	// (on French layout: & é " ' are the physical keys 1 2 3 4)
	if (event.type == SDL_EVENT_KEY_DOWN && workspace_widget_)
	{
		static const SDL_Scancode modeScancodes[4] = {
			SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4
		};
		static const char* modeKeys[4] = { "1", "2", "3", "4" };

		for (int i = 0; i < 4; ++i)
		{
			if (event.key.scancode == modeScancodes[i])
			{
				workspace_widget_->sendKeyEvent(modeKeys[i]);
				break;
			}
		}
	}

}


// ====== Rendering ====== //

void SDL_ApplicationWindow::renderFrame()
{
	// ===== Frame Counter & Debug Logging =====
	frameCounter();

	// ===== Swapchain Setup & Image Acquisition =====
	swapchainSetup(render_frame_count);
	if (!frame_acquisition_succeeded_)
	{
		return;
	}

	// ===== Render Pass, Viewport & Scissor Setup =======
	if (!renderPassViewportAndScissorSetup())
	{
		return;
	}

	// Get command buffer and window dimensions for state-based rendering
	VkCommandBuffer commandBuffer = vk_command_buffers_[current_image_index_];
	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);

	// ------- Splitter interactions (before preparePanels so same-frame data is used) ------- //
	if (ui_manager_)
	{
		float mouseXf = 0.0f, mouseYf = 0.0f;
		SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseXf, &mouseYf);
		const bool isLeftButtonDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;

		// In Maximized mode, splitter_list_ is frozen in 2560-space (actual logical window coords).
		// SDL_GetMouseState already returns logical coordinates → no scaling needed.
		// (The old scaling from 2560→1920 broke hit detection once splitters were frozen in 2560-space.)

		ui_manager_->enableSplitterMouseInteractions(static_cast<int>(mouseXf), static_cast<int>(mouseYf), isLeftButtonDown);
		updateMouseState(static_cast<int>(mouseXf), static_cast<int>(mouseYf));
	}

	// ------- Main rendering logic ------- //
	// ONLY render content when in Init state - otherwise just clear to black
	if (current_state_ == &state_init_)
	{
		if (ui_manager_)
		{
			ui_manager_->bindWorkSpaceSizeToSplitterInteractions();
			ui_manager_->bindPanelsToSplitters(getWindowRenderState());

			if (ui_manager_->consumePendingCEFRepaintRequest() || pending_window_resize_sync_)
			{
				pending_window_resize_sync_ = false;
				if (panel_resizing_logic_)
				{
					panel_resizing_logic_->cacheUIPanelFrameDatas();
					int currentW = 0, currentH = 0;
					SDL_GetWindowSize(window_, &currentW, &currentH);
					panel_resizing_logic_->syncBrowserPanelLayoutFromCurrentFrames(
						getWindowRenderState(),
						currentW, currentH,
						reference_window_width_, reference_window_height_);
				}
				requestBrowserRepaint();
			}
		}

		activateDebugRender();

		drawThreeDScreenOnWorkspace();
		
		preparePanels();

			if (app_border_)
			{

				// ------------------ architectural explanation ---------------------
				// Set the borders of the UIManager inside which the UI will be drawn.
				// In PanelMapBuilder::drawInsideAppBorders, the Vulkan viewport is set with an offset:
				
				//   viewport.x = static_cast<float>(appBorderLeft);
				//   viewport.y = static_cast<float>(appBorderTop);
				//   viewport.width = static_cast<float>(appBorderWidth);
				//   viewport.height = static_cast<float>(appBorderHeight);
				
				// Considering that each panel is drawn individually, 
				// this translates (shifts) the entire UI group as a whole: NDC(-1,-1) maps to (borderLeft, borderTop) on screen.
				// All panels are part of the same render pass and draw call group.
				// An offset can be applied here (e.g. getLeft()+N) to displace the UI without any clipping.

				// if you write getLeft() + 50 you will see an offset 
				// -------------------- end of explanation ---------------------

				ui_manager_->setBorders(
					app_border_->getLeft(), app_border_->getTop(),
					app_border_->getWidth(), app_border_->getHeight());
				borders_set_for_init_ = true;
			}

			ui_manager_->drawReduceScreenUIpanelsInsideBorders(commandBuffer, prepared_drawable_width_, prepared_drawable_height_,
			prepared_panel_frame_data_map_, prepared_skip_texture_rebuild_, window_,
			reference_window_width_, reference_window_height_);

			ui_manager_->drawSplitters(commandBuffer, prepared_drawable_width_, prepared_drawable_height_);
	}

	// ------ when state is Maximized for SDL3 window
	else if (current_state_ == &state_maximized_)
	{
		if (ui_manager_)
		{
			int fullscreenW = 0, fullscreenH = 0;
			SDL_GetWindowSize(window_, &fullscreenW, &fullscreenH);

			ui_manager_->FreezeCoordinatesForFullscreen(fullscreenW, fullscreenH);

			ui_manager_->bindWorkSpaceSizeToSplitterInteractions();

			ui_manager_->bindFullScreenPanelsToSplitters(getWindowRenderState());

			if (ui_manager_->consumePendingCEFRepaintRequest())
			{
				if (panel_resizing_logic_)
				{
					panel_resizing_logic_->cacheUIPanelFrameDatas();
					int currentW = 0, currentH = 0;
					SDL_GetWindowSize(window_, &currentW, &currentH);

					panel_resizing_logic_->syncBrowserFullScreenPanelLayout(
						getWindowRenderState(),
						currentW, currentH,
						currentW, currentH);
				}
				requestBrowserRepaint();
			}
		}

		activateDebugRender();

		drawThreeDScreenOnWorkspace();

		preparePanels();

		if (ui_manager_ && app_border_)
		{
			ui_manager_->setBorders(
				app_border_->getLeft(), app_border_->getTop(),
				app_border_->getWidth(), app_border_->getHeight());

			ui_manager_->renderFullScreenUIPanelsInsideBorders(commandBuffer, prepared_drawable_width_, prepared_drawable_height_,
				prepared_skip_texture_rebuild_, window_);

			ui_manager_->drawSplittersFullScreen(commandBuffer, prepared_drawable_width_, prepared_drawable_height_,
				window_, app_border_->getReferenceWindowWidth(), app_border_->getReferenceWindowHeight());
		}
	}

	// ------ ScalingUp : window is being enlarged toward fullscreen
	else if (current_state_ == &state_scaling_up_)
	{
		if (ui_manager_ && app_border_)
		{
			ui_manager_->prepareUiPanelsForFullscreen(
				prepared_drawable_width_, prepared_drawable_height_,
				prepared_panel_frame_data_map_, window_,
				app_border_->getReferenceWindowWidth(), app_border_->getReferenceWindowHeight());
		}
		StateTransition(&state_scaling_up_, &state_maximized_);
	}

	// ------ ScalingDown : window is being restored from fullscreen toward init
	else if (current_state_ == &state_scaling_down_)
	{
		if (ui_manager_ && app_border_)
		{
			ui_manager_->cleanUpDatasBeforeDrawingForReducedScreen();
			ui_manager_->requestPendingCEFRepaint();
		}
		StateTransition(&state_scaling_down_, &state_reduced_);
	}

	else if (current_state_ == &state_reduced_)
	{
		activateDebugRender();

		drawThreeDScreenOnWorkspace();

		preparePanels();

		if (ui_manager_ && app_border_)
		{
			ui_manager_->setBorders(
				app_border_->getLeft(), app_border_->getTop(),
				app_border_->getWidth(), app_border_->getHeight());

			ui_manager_->bindWorkSpaceSizeToSplitterInteractions();

			ui_manager_->bindPanelsToSplitters(getWindowRenderState());

			if (ui_manager_->consumePendingCEFRepaintRequest() || pending_window_resize_sync_)
			{
				pending_window_resize_sync_ = false;
				if (panel_resizing_logic_)
				{
					panel_resizing_logic_->cacheUIPanelFrameDatas();
					int currentW = 0, currentH = 0;
					SDL_GetWindowSize(window_, &currentW, &currentH);

					panel_resizing_logic_->syncBrowserFullScreenPanelLayout(
						getWindowRenderState(),
						currentW, currentH,
						currentW, currentH);
				}
				requestBrowserRepaint();
			}

			ui_manager_->drawReduceScreenUIpanelsInsideBorders(commandBuffer, prepared_drawable_width_, prepared_drawable_height_,
				prepared_panel_frame_data_map_, prepared_skip_texture_rebuild_, window_,
				app_border_->getReferenceWindowWidth(), app_border_->getReferenceWindowHeight());

			ui_manager_->drawSplittersReducedScreenSize(commandBuffer, prepared_drawable_width_, prepared_drawable_height_,
				window_, app_border_->getReferenceWindowWidth(), app_border_->getReferenceWindowHeight());
		}
	}

	// functionnalities that dosn't depend on window_state 
	if (ui_manager_ && browser_)
	{
		float mouseXf = 0.0f, mouseYf = 0.0f;
		SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseXf, &mouseYf);
		const bool isLeftButtonDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
		const bool isRightButtonDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
		ui_manager_->forwardMouseEventsToPanels(static_cast<int>(mouseXf), static_cast<int>(mouseYf), isLeftButtonDown, isRightButtonDown, browser_);
	}

	if (contextual_menu_visible_ && contextual_menu_above_gui_)
	{
		float mouseXf = 0.0f, mouseYf = 0.0f;
		SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseXf, &mouseYf);
		const bool isLeftButtonDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;
		const bool isRightButtonDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) != 0;
		contextual_menu_above_gui_->handleMouseEvent(static_cast<int>(mouseXf), static_cast<int>(mouseYf), isLeftButtonDown, isRightButtonDown);
	}

	if (debug_tools_ && ui_manager_)
	{
		debug_tools_->drawDebugTools(commandBuffer, width, height, &ui_manager_->getWorkSpace());
	}

	if (!finalizeAndSubmitCommandBuffer(commandBuffer))
	{
		return;
	}

	// --- functionnalities when working with 3D screen (independent of window state)  ---
	// --- the mouse state let us know if the mouse is above the 3D screen / the workspace or not ---
	if (current_mouse_state_ == &mouse_state_above_workspace_)
	{
		float mouseXf = 0.0f, mouseYf = 0.0f;
		SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(&mouseXf, &mouseYf);
		const int mouseX = static_cast<int>(mouseXf);
		const int mouseY = static_cast<int>(mouseYf);
		const bool middleDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) != 0;
		const bool leftDown = (mouseButtons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) != 0;

		if (camera_control_)
		{
			if (middleDown && !prev_middle_button_down_)
			{
				camera_control_->onMiddleButtonDown(mouseX, mouseY);
			}
			else if (!middleDown && prev_middle_button_down_)
			{
				camera_control_->onMiddleButtonUp();
			}

			if (middleDown)
			{
				camera_control_->onMouseMove(mouseX, mouseY);
			}

			if (pending_wheel_delta_ != 0.0f)
			{
				camera_control_->onWheel(pending_wheel_delta_);
				pending_wheel_delta_ = 0.0f;
			}
		}

		// ----- raycast performing when clicking on 3D screen -----
		if (pending_left_click_ && raycast_perform_ && camera_ && vk_scene_ && ui_manager_)
		{
			pending_left_click_ = false;
			updateWorkspaceProjectionData();
			
			float aspectRatio = (workspaceHeight_ > 0)
				? static_cast<float>(workspaceWidth_) / static_cast<float>(workspaceHeight_)
				: 1.0f;

			viewMatrix_ = camera_->getViewMatrix();
			projectionMatrix_ = camera_->getProjectionMatrix(aspectRatio);

			const std::list<ThreeDObject*>& objList = vk_scene_->getObjectsRef();
			std::vector<ThreeDObject*> objects(objList.begin(), objList.end());

			raycast_perform_->performRaycast(
				mouseX, mouseY,
				workspaceX_, workspaceY_,
				workspaceWidth_, workspaceHeight_,
				viewMatrix_, projectionMatrix_,
				objects);

			const std::vector<ThreeDObject*>& selectedObjects = raycast_perform_->getLastHitObjects(); 
			std::list<ThreeDObject*> selectedObjectsList(selectedObjects.begin(), selectedObjects.end());

			std::cout << " [SDL_ApplicationWindow RenderFrame] the selected objects are : " << std::endl;

			vk_scene_->setSelectedObjects(selectedObjectsList);

			if (data_holders_)
			{
				data_holders_->setSelectedObjectsForDataHolders(selectedObjectsList);
				data_holders_->computeNewDataToDataHolders();
			}
		}
		prev_middle_button_down_ = middleDown;
		prev_left_button_down_ = leftDown;
	}

	if (!vk_scene_->getSelectedObjects().empty())
	{
		updateWorkspaceProjectionData();

		// Recompute view/proj every frame so the gizmo follows the camera
		if (camera_ && workspaceHeight_ > 0)
		{
			float aspectRatio = static_cast<float>(workspaceWidth_) / static_cast<float>(workspaceHeight_);
			viewMatrix_ = camera_->getViewMatrix();
			projectionMatrix_ = camera_->getProjectionMatrix(aspectRatio);
		}

		// Start an ImGui frame so ImGuizmo can draw into its overlay draw list
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		// Render gizmo for the selected objects
		Guizmo::renderGizmoForObject(vk_scene_->getSelectedObjects(), ImGuizmo::TRANSLATE, viewMatrix_, projectionMatrix_, 
		ImVec2(static_cast<float>(workspaceX_), static_cast<float>(workspaceY_)), ImVec2(static_cast<float>(workspaceWidth_), static_cast<float>(workspaceHeight_)));

		// Submit ImGui draw data (including the gizmo) as an overlay render pass
		renderImGui();
	}

	// -------- render Widgets above Workspace Screen -------- //

	if (workspace_widget_ && ui_manager_ && widget_command_buffer_ != VK_NULL_HANDLE)
	{
		workspace_widget_->uploadPaintBuffer();

		const SIMILI::Frontend::WorkSpace& ws = ui_manager_->getWorkSpace();
		if (ws.isValid())
		{
			int drawableW = 0, drawableH = 0;
			int logicalW = 0, logicalH = 0;
			SDL_GetWindowSizeInPixels(window_, &drawableW, &drawableH);
			SDL_GetWindowSize(window_, &logicalW, &logicalH);
			if (logicalW > 0 && logicalH > 0)
			{
				const float scaleX = static_cast<float>(drawableW) / static_cast<float>(logicalW);
				const float scaleY = static_cast<float>(drawableH) / static_cast<float>(logicalH);
				const int wsXDrawable = static_cast<int>(static_cast<float>(ws.getX()) * scaleX);
				const int wsYDrawable = static_cast<int>(static_cast<float>(ws.getY()) * scaleY);
				renderWorkspaceWidgets(wsXDrawable, wsYDrawable, drawableW, drawableH);
			}
		}
	}

	// ----- render ContextualMenu above everything, GUI and workspace included -----
	if (contextual_menu_visible_ && contextual_menu_above_gui_ && contextual_menu_command_buffer_ != VK_NULL_HANDLE)
	{
		contextual_menu_above_gui_->uploadPaintBuffer();

		int drawableW = 0, drawableH = 0;
		SDL_GetWindowSizeInPixels(window_, &drawableW, &drawableH);
		if (drawableW > 0 && drawableH > 0)
		{
			RenderContextualMenuAboveUI(drawableW, drawableH);
		}
	}

	presentToScreen();
}


void SDL_ApplicationWindow::drawThreeDScreenOnWorkspace()
{
	if (camera_ && vk_scene_ && ui_manager_ && vk_command_buffers_.size() > current_image_index_)
	{
		const SIMILI::Frontend::WorkSpace& ws = ui_manager_->getWorkSpace();
		if (ws.isValid())
		{
			int drawableW = 0, drawableH = 0;
			int logicalW = 0, logicalH = 0;
			SDL_GetWindowSizeInPixels(window_, &drawableW, &drawableH);
			SDL_GetWindowSize(window_, &logicalW, &logicalH);

			if (logicalW > 0 && logicalH > 0)
			{
				const float scaleX = static_cast<float>(drawableW) / static_cast<float>(logicalW);
				const float scaleY = static_cast<float>(drawableH) / static_cast<float>(logicalH);

				const int vpX = static_cast<int>(ws.getX() * scaleX);
				const int vpY = static_cast<int>(ws.getY() * scaleY);
				const int vpW = static_cast<int>(ws.getWidth() * scaleX);
				const int vpH = static_cast<int>(ws.getHeight() * scaleY);

				camera_->projectSceneViewOnSDL3WorkSpace(
					vk_scene_,
					vk_command_buffers_[current_image_index_],
					vpX, vpY, vpW, vpH);
			}
		}
	}
}


void SDL_ApplicationWindow::preparePanels()
{
	if (!ui_manager_ || vk_command_buffers_.size() <= current_image_index_)
	{
		return;
	}

	SDL_GetWindowSizeInPixels(window_, &prepared_drawable_width_, &prepared_drawable_height_);

	prepared_panel_frame_data_map_.clear();

	if (prepared_panel_frame_data_map_.empty())
	{
		prepared_panel_frame_data_map_ = ui_manager_->getUIPanelFrameDatas();
	}

	{
		std::set<std::string> seenBounds;
		for (auto it = prepared_panel_frame_data_map_.begin(); it != prepared_panel_frame_data_map_.end(); )
		{
			const auto& d = it->second;
			std::string key = std::to_string(d.relativeX) + "," + std::to_string(d.relativeY)
			                + "," + std::to_string(d.width) + "," + std::to_string(d.height);
			if (!seenBounds.insert(key).second)
			{
				it = prepared_panel_frame_data_map_.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	prepared_skip_texture_rebuild_ = false;

	if (vk_context_)
		uploadCEFPaintBuffer();
}

void SDL_ApplicationWindow::startSplitter()
{

	if (ui_manager_ && vk_context_ && vk_render_pass_ != VK_NULL_HANDLE && vulkan_pipelines_)
	{
		ui_manager_->setVKContext(vk_context_);
		ui_manager_->setVulkanPipelines(vulkan_pipelines_);
		ui_manager_->setRenderPass(vk_render_pass_);
		std::cout << "[SDL_ApplicationWindow] UIManager initialized with Vulkan resources" << std::endl;

		if (!ui_manager_vulkan_initialized_)
		{
			ui_manager_vulkan_initialized_ = true;
		}
	}

	if (debug_tools_ && vk_context_ && vk_render_pass_ != VK_NULL_HANDLE && vulkan_pipelines_ && app_border_)
	{
		debug_tools_->initialize(vk_context_, vk_render_pass_, vulkan_pipelines_, app_border_);
	}
}

// ===== End of rendering =====

CefRefPtr<SIMILICefClient> SDL_ApplicationWindow::getCefClient()
{
	if (!simili_cef_client_)
	{
		simili_cef_client_ = new SIMILICefClient(this);
	}
	return simili_cef_client_;
}

void SDL_ApplicationWindow::initializeDefaultUIPanels()
{
	if (ui_manager_)
	{
		ui_manager_->initializeDefaultUIPanels();
		std::cout << "[SDL_ApplicationWindow] Default UI panels initialized" << std::endl;
	}
	else
	{
		std::cerr << "[SDL_ApplicationWindow] initializeDefaultUIPanels: ui_manager_ is null" << std::endl;
	}
}

void SDL_ApplicationWindow::forceCaptureIFramePositions()
{
	if (!frame_datas_)
	{
		std::cerr << "[SDL_ApplicationWindow] forceCaptureIFramePositions: frame_datas_ not available" << std::endl;
		return;
	}

	SDL_Window* sdlWindow = getHandle();
	if (!sdlWindow)
	{
		std::cerr << "[SDL_ApplicationWindow] forceCaptureIFramePositions: SDL window handle not available" << std::endl;
		return;
	}

	frame_datas_->catchFrameData(sdlWindow);

	if (ui_manager_)
	{
		ui_manager_->syncFrameDatas(frame_datas_, sdlWindow);
		std::cout << "[SDL_ApplicationWindow] FORCE - UI panel frame data synced to UIManager" << std::endl;
	}
	else
	{
		std::cerr << "[SDL_ApplicationWindow] FORCE - WARNING: ui_manager_ is null, cannot sync frame data" << std::endl;
	}

	if (panel_resizing_logic_)
	{
		panel_resizing_logic_->cacheUIPanelFrameDatas();
	}

	updateFrameDatas(frame_datas_);
}

void SDL_ApplicationWindow::activateDebugRender(){
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

	if (!initializeImGui())
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to initialize ImGui" << std::endl;
		return false;
	}

	VkCommandBufferAllocateInfo widgetCmdAlloc{};
	widgetCmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	widgetCmdAlloc.commandPool = vk_command_pool_;
	widgetCmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	widgetCmdAlloc.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(vk_context_->getDevice(), &widgetCmdAlloc, &widget_command_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to allocate widget command buffer" << std::endl;
		return false;
	}

	VkCommandBufferAllocateInfo contextualMenuCmdAlloc{};
	contextualMenuCmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	contextualMenuCmdAlloc.commandPool = vk_command_pool_;
	contextualMenuCmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	contextualMenuCmdAlloc.commandBufferCount = 1;
	
	if (vkAllocateCommandBuffers(vk_context_->getDevice(), &contextualMenuCmdAlloc, &contextual_menu_command_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to allocate contextual menu command buffer" << std::endl;
		return false;
	}

	std::cout << "[SDL_ApplicationWindow] Vulkan initialized successfully" << std::endl;
	return true;
}

// ===== ImGui Integration ===== //

bool SDL_ApplicationWindow::initializeImGui()
{
	VkDevice device = vk_context_->getDevice();

	// Descriptor pool for ImGui
	VkDescriptorPoolSize poolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
	};
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 16;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = poolSizes;
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &imgui_descriptor_pool_) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] initializeImGui: failed to create descriptor pool" << std::endl;
		return false;
	}

	// ImGui render pass: LOAD_OP_LOAD so it overlays on top of the already-rendered frame
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpInfo{};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = 1;
	rpInfo.pAttachments = &colorAttachment;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;
	rpInfo.dependencyCount = 1;
	rpInfo.pDependencies = &dep;
	if (vkCreateRenderPass(device, &rpInfo, nullptr, &imgui_render_pass_) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] initializeImGui: failed to create render pass" << std::endl;
		return false;
	}

	// Allocate a dedicated command buffer for ImGui submission
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk_command_pool_;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(device, &allocInfo, &imgui_command_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] initializeImGui: failed to allocate command buffer" << std::endl;
		return false;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO().IniFilename = nullptr;

	ImGui_ImplSDL3_InitForVulkan(window_);

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = vk_context_->getInstance();
	initInfo.PhysicalDevice = vk_context_->getPhysicalDevice();
	initInfo.Device = device;
	initInfo.QueueFamily = vk_context_->getGraphicsQueueFamily();
	initInfo.Queue = vk_context_->getGraphicsQueue();
	initInfo.DescriptorPool = imgui_descriptor_pool_;
	initInfo.RenderPass = imgui_render_pass_;
	initInfo.MinImageCount  = 2;
	initInfo.ImageCount = static_cast<uint32_t>(vk_swapchain_images_.size());
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	ImGui_ImplVulkan_Init(&initInfo);

	imgui_initialized_ = true;
	std::cout << "[SDL_ApplicationWindow] ImGui+ImGuizmo initialized" << std::endl;
	return true;
}

void SDL_ApplicationWindow::shutdownImGui()
{
	if (!imgui_initialized_) return;
	vkDeviceWaitIdle(vk_context_->getDevice());
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	if (imgui_render_pass_ != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(vk_context_->getDevice(), imgui_render_pass_, nullptr);
		imgui_render_pass_ = VK_NULL_HANDLE;
	}
	if (imgui_descriptor_pool_ != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(vk_context_->getDevice(), imgui_descriptor_pool_, nullptr);
		imgui_descriptor_pool_ = VK_NULL_HANDLE;
	}
	imgui_initialized_ = false;
	std::cout << "[SDL_ApplicationWindow] ImGui shut down" << std::endl;
}

void SDL_ApplicationWindow::renderImGui()
{
	if (!imgui_initialized_) return;

	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();
	if (!drawData || drawData->TotalVtxCount == 0) return;

	VkDevice device = vk_context_->getDevice();
	VkCommandBuffer cmd = imgui_command_buffer_;

	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);

	// Build a temporary framebuffer for this overlay pass (color only, no depth)
	VkImageView colorView = vk_swapchain_image_views_[current_image_index_];
	VkFramebufferCreateInfo fbInfo{};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.renderPass = imgui_render_pass_;
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &colorView;
	fbInfo.width  = static_cast<uint32_t>(width);
	fbInfo.height = static_cast<uint32_t>(height);
	fbInfo.layers = 1;
	VkFramebuffer fb = VK_NULL_HANDLE;
	vkCreateFramebuffer(device, &fbInfo, nullptr, &fb);

	VkRenderPassBeginInfo rpBegin{};
	rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBegin.renderPass = imgui_render_pass_;
	rpBegin.framebuffer = fb;
	rpBegin.renderArea.offset = {0, 0};
	rpBegin.renderArea.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
	rpBegin.clearValueCount = 0;
	vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

	ImGui_ImplVulkan_RenderDrawData(drawData, cmd);

	vkCmdEndRenderPass(cmd);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vk_context_->getGraphicsQueue());

	vkDestroyFramebuffer(device, fb, nullptr);
}

void SDL_ApplicationWindow::renderWorkspaceWidgets(int wsX, int wsY, int drawableW, int drawableH)
{
	if (!workspace_widget_ || widget_command_buffer_ == VK_NULL_HANDLE) return;
	if (imgui_render_pass_ == VK_NULL_HANDLE) return;

	VkDevice device = vk_context_->getDevice();
	VkCommandBuffer cmd = widget_command_buffer_;

	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	VkImageView colorView = vk_swapchain_image_views_[current_image_index_];
	VkFramebufferCreateInfo fbInfo{};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.renderPass = imgui_render_pass_;
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &colorView;
	fbInfo.width  = static_cast<uint32_t>(drawableW);
	fbInfo.height = static_cast<uint32_t>(drawableH);
	fbInfo.layers = 1;
	VkFramebuffer fb = VK_NULL_HANDLE;
	vkCreateFramebuffer(device, &fbInfo, nullptr, &fb);

	VkRenderPassBeginInfo rpBegin{};
	rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBegin.renderPass = imgui_render_pass_;
	rpBegin.framebuffer = fb;
	rpBegin.renderArea.offset = { 0, 0 };
	rpBegin.renderArea.extent = { static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH) };
	rpBegin.clearValueCount = 0;
	vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport vp{};
	vp.x = 0.0f; vp.y = 0.0f;
	vp.width  = static_cast<float>(drawableW);
	vp.height = static_cast<float>(drawableH);
	vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &vp);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH) };
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	workspace_widget_->draw(cmd, wsX, wsY, drawableW, drawableH);

	vkCmdEndRenderPass(cmd);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vk_context_->getGraphicsQueue());

	vkDestroyFramebuffer(device, fb, nullptr);
}

void SDL_ApplicationWindow::RenderContextualMenuAboveUI(int drawableW, int drawableH)
{
	if (!contextual_menu_above_gui_ || contextual_menu_command_buffer_ == VK_NULL_HANDLE) return;
	if (imgui_render_pass_ == VK_NULL_HANDLE) return;

	VkDevice device = vk_context_->getDevice();
	VkCommandBuffer cmd = contextual_menu_command_buffer_;

	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	VkImageView colorView = vk_swapchain_image_views_[current_image_index_];
	VkFramebufferCreateInfo fbInfo{};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.renderPass = imgui_render_pass_;
	fbInfo.attachmentCount = 1;
	fbInfo.pAttachments = &colorView;
	fbInfo.width  = static_cast<uint32_t>(drawableW);
	fbInfo.height = static_cast<uint32_t>(drawableH);
	fbInfo.layers = 1;
	VkFramebuffer fb = VK_NULL_HANDLE;
	vkCreateFramebuffer(device, &fbInfo, nullptr, &fb);

	VkRenderPassBeginInfo rpBegin{};
	rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBegin.renderPass = imgui_render_pass_;
	rpBegin.framebuffer = fb;
	rpBegin.renderArea.offset = { 0, 0 };
	rpBegin.renderArea.extent = { static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH) };
	rpBegin.clearValueCount = 0;
	vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport vp{};
	vp.x = 0.0f; vp.y = 0.0f;
	vp.width  = static_cast<float>(drawableW);
	vp.height = static_cast<float>(drawableH);
	vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &vp);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH) };
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// size of the menu
	int menuW = contextual_menu_above_gui_->getWidgetWidth();
	int menuH = contextual_menu_above_gui_->getWidgetHeight();

	// Convert logical click menu_x_ and menu_y_ coordinates to drawable coordinates
	int logicalW = 0, logicalH = 0;
	SDL_GetWindowSize(window_, &logicalW, &logicalH);
	float scaleX = (logicalW > 0) ? (static_cast<float>(drawableW) / static_cast<float>(logicalW)) : 1.0f;
	float scaleY = (logicalH > 0) ? (static_cast<float>(drawableH) / static_cast<float>(logicalH)) : 1.0f;

	int wsX = static_cast<int>(contextual_menu_above_gui_->getMenuX() * scaleX);
	int wsY = static_cast<int>(contextual_menu_above_gui_->getMenuY() * scaleY);

	// Clamp within safe screen boundaries
	if (wsX + menuW > drawableW) {
		wsX = drawableW - menuW;
	}
	if (wsY + menuH > drawableH) {
		wsY = drawableH - menuH;
	}
	if (wsX < 0) wsX = 0;
	if (wsY < 0) wsY = 0;

	contextual_menu_above_gui_->draw(cmd, wsX, wsY, drawableW, drawableH);

	vkCmdEndRenderPass(cmd);
	vkEndCommandBuffer(cmd);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vk_context_->getGraphicsQueue());

	vkDestroyFramebuffer(device, fb, nullptr);
}

void SDL_ApplicationWindow::cleanupVulkan()
{
	if (!vk_context_)
	{
		return;
	}

	shutdownImGui();

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

	if (vk_depth_image_view_ != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, vk_depth_image_view_, nullptr);
		vk_depth_image_view_ = VK_NULL_HANDLE;
	}
	if (vk_depth_image_ != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, vk_depth_image_, nullptr);
		vk_depth_image_ = VK_NULL_HANDLE;
	}
	if (vk_depth_image_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, vk_depth_image_memory_, nullptr);
		vk_depth_image_memory_ = VK_NULL_HANDLE;
	}

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

	// if (ui_handler_)
	// {
	// 	UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
	// 	handler->updateWindowSize(logicalWidth, logicalHeight);
	// }

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
	
	// if (ui_handler_)
	// {
	// 	UIHandler* handler = static_cast<UIHandler*>(ui_handler_);
		
	// 	Splitter* splitter = handler->getSplitter();
	// 	if (splitter)
	// 	{
	// 		std::cout << "[SDL_ApplicationWindow] Forcing splitter layout refresh after swapchain recreation" << std::endl;
	// 		// splitter->forceRefreshLayout();
	// 	}
	// }
	
	return true;
}

void SDL_ApplicationWindow::handleSwapchainRecreation()
{
	if (!swapchain_needs_recreation_)
	{
		return;
	}

	VkDevice device = vk_context_->getDevice();

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
}

void SDL_ApplicationWindow::swapchainSetup(int render_frame_count)
{
	frame_acquisition_succeeded_ = false;

	if (swapchain_needs_recreation_)
	{
		handleSwapchainRecreation();
	}

	// ===== Context & Swapchain Validation =====
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

	// ===== Swapchain Image Acquisition =====
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

	frame_acquisition_succeeded_ = true;
}

bool SDL_ApplicationWindow::renderPassViewportAndScissorSetup()
{
	// ===== Command Buffer Preparation =====
	VkCommandBuffer commandBuffer = vk_command_buffers_[current_image_index_];
	vkResetCommandBuffer(commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to begin command buffer" << std::endl;
		return false;
	}

	// ===== Render Pass Configuration =====
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vk_render_pass_;
	renderPassInfo.framebuffer = vk_framebuffers_[current_image_index_];
	renderPassInfo.renderArea.offset = {0, 0};

	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);
	renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

	VkClearValue clearValues[2];
	clearValues[0] = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	clearValues[1].depthStencil = { 1.0f, 0 };
	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// ===== Viewport & Scissor Setup =====
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

	return true;
}

bool SDL_ApplicationWindow::finalizeAndSubmitCommandBuffer(VkCommandBuffer commandBuffer)
{
	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
	{
		std::cerr << "[SDL_ApplicationWindow] Failed to end command buffer" << std::endl;
		return false;
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

	{
		std::lock_guard<std::mutex> qlock(queue_mutex_);
		if (vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, vk_in_flight_fences_[current_frame_]) != VK_SUCCESS)
		{
			std::cerr << "[SDL_ApplicationWindow] Failed to submit draw command buffer" << std::endl;
			return false;
		}
	}

	vk_image_fences_[current_image_index_] = vk_in_flight_fences_[current_frame_];
	return true;
}

void SDL_ApplicationWindow::presentToScreen()
{
	VkSemaphore signalSemaphores[] = {vk_render_finished_semaphores_[current_frame_]};
	
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = {vk_swapchain_};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &current_image_index_;

	VkResult result = vkQueuePresentKHR(vk_context_->getPresentQueue(), &presentInfo);
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


// ==== private methods ==== //

void SDL_ApplicationWindow::StateTransition(SDL_State* from, SDL_State* to)
{
	if (from)
		from->leave_state();

	current_state_ = to;

	if (to)
		to->enter_state();
}

void SDL_ApplicationWindow::updateMouseState(int mouseX, int mouseY)
{
	if (!ui_manager_)
		return;

	bool isOverUI = false;

	if (contextual_menu_visible_ && contextual_menu_above_gui_ && window_)
	{
		int logicalW = 0, logicalH = 0;
		SDL_GetWindowSize(window_, &logicalW, &logicalH);
		int drawableW = 0, drawableH = 0;
		SDL_GetWindowSizeInPixels(window_, &drawableW, &drawableH);
		float scaleX = (logicalW > 0) ? (static_cast<float>(drawableW) / static_cast<float>(logicalW)) : 1.0f;
		float scaleY = (logicalH > 0) ? (static_cast<float>(drawableH) / static_cast<float>(logicalH)) : 1.0f;

		int menuX = contextual_menu_above_gui_->getMenuX();
		int menuY = contextual_menu_above_gui_->getMenuY();
		int logicalMenuW = static_cast<int>(contextual_menu_above_gui_->getWidgetWidth() / scaleX);
		int logicalMenuH = static_cast<int>(contextual_menu_above_gui_->getWidgetHeight() / scaleY);

		if (mouseX >= menuX && mouseX < menuX + logicalMenuW &&
			mouseY >= menuY && mouseY < menuY + logicalMenuH)
		{
			isOverUI = true;
		}
	}

	if (isOverUI)
	{
		if (current_mouse_state_ != &mouse_state_above_ui_panel_)
		{
			current_mouse_state_->onExit();
			current_mouse_state_ = &mouse_state_above_ui_panel_;
			current_mouse_state_->onEnter();
		}
	}
	else
	{
		const auto& workspace = ui_manager_->getWorkSpace();
		if (!workspace.isValid())
			return;

		const bool isAbove = (mouseX >= workspace.getX()
			&& mouseX <= workspace.getX() + workspace.getWidth()
			&& mouseY >= workspace.getY()
			&& mouseY <= workspace.getY() + workspace.getHeight());

		if (isAbove && current_mouse_state_ != &mouse_state_above_workspace_)
		{
			current_mouse_state_->onExit();
			current_mouse_state_ = &mouse_state_above_workspace_;
			current_mouse_state_->onEnter();
		}
		else if (!isAbove && current_mouse_state_ != &mouse_state_outside_workspace_)
		{
			current_mouse_state_->onExit();
			current_mouse_state_ = &mouse_state_outside_workspace_;
			current_mouse_state_->onEnter();
		}
	}
}

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




// ===== Vulkan Resource Creation Methods ===== //

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

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
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
	VkDevice device = vk_context_->getDevice();

	if (vk_depth_image_view_ != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, vk_depth_image_view_, nullptr);
		vk_depth_image_view_ = VK_NULL_HANDLE;
	}
	if (vk_depth_image_ != VK_NULL_HANDLE)
	{
		vkDestroyImage(device, vk_depth_image_, nullptr);
		vk_depth_image_ = VK_NULL_HANDLE;
	}
	if (vk_depth_image_memory_ != VK_NULL_HANDLE)
	{
		vkFreeMemory(device, vk_depth_image_memory_, nullptr);
		vk_depth_image_memory_ = VK_NULL_HANDLE;
	}

	int width, height;
	SDL_GetWindowSizeInPixels(window_, &width, &height);

	VkImageCreateInfo depthImageInfo{};
	depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	depthImageInfo.mipLevels = 1;
	depthImageInfo.arrayLayers = 1;
	depthImageInfo.format = VK_FORMAT_D32_SFLOAT;
	depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &depthImageInfo, nullptr, &vk_depth_image_) != VK_SUCCESS)
	{
		return false;
	}

	VkMemoryRequirements depthMemReq;
	vkGetImageMemoryRequirements(device, vk_depth_image_, &depthMemReq);
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProps);
	uint32_t depthMemIdx = 0;
	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if ((depthMemReq.memoryTypeBits & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
		{
			depthMemIdx = i;
			break;
		}
	}
	VkMemoryAllocateInfo depthAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	depthAllocInfo.allocationSize = depthMemReq.size;
	depthAllocInfo.memoryTypeIndex = depthMemIdx;
	if (vkAllocateMemory(device, &depthAllocInfo, nullptr, &vk_depth_image_memory_) != VK_SUCCESS)
	{
		return false;
	}
	vkBindImageMemory(device, vk_depth_image_, vk_depth_image_memory_, 0);

	VkImageViewCreateInfo depthViewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	depthViewInfo.image = vk_depth_image_;
	depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
	depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
	if (vkCreateImageView(device, &depthViewInfo, nullptr, &vk_depth_image_view_) != VK_SUCCESS)
	{
		return false;
	}

	vk_framebuffers_.resize(vk_swapchain_image_views_.size());

	for (size_t i = 0; i < vk_swapchain_image_views_.size(); i++)
	{
		VkImageView attachments[] = { vk_swapchain_image_views_[i], vk_depth_image_view_ };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = vk_render_pass_;
		framebufferInfo.attachmentCount = 2;
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

void SDL_ApplicationWindow::frameCounter()
{
	// ===== Frame Counter & Debug Logging =====
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
}

void SDL_ApplicationWindow::updateWorkspaceProjectionData()
{

	const auto& workspace = ui_manager_->getWorkSpace();

	workspaceX_ = workspace.getX();
	workspaceY_ = workspace.getY();
	workspaceWidth_ = workspace.getWidth();
	workspaceHeight_ = workspace.getHeight();
}

// In SDL_ApplicationWindow.cpp
DataHolders& SDL_ApplicationWindow::getDataHolders() 
{
    return *data_holders_;
}
void SDL_ApplicationWindow::setDataHolders(DataHolders* dataHolders) 
{
    data_holders_ = dataHolders;
}

void SDL_ApplicationWindow::onObjectSelectedFromHierarchy(ThreeDObject* object)
{
	if (!object) return;

	selectedObjectsList_.clear();
	selectedObjectsList_.push_back(object);

	if (data_holders_)
	{
		data_holders_->setSelectedObjectsForDataHolders(selectedObjectsList_);
		data_holders_->computeNewDataToDataHolders();
	}

	if(vk_scene_)
	{
		vk_scene_->setSelectedObjects(selectedObjectsList_);
	}

	std::cout << "[SDL_ApplicationWindow] Object selected from hierarchy: " << object->getName() << std::endl;
}