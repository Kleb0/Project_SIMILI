#pragma once

#include "include/cef_client.h"
#include "include/cef_browser.h"
#include "include/cef_render_handler.h"
#include "include/cef_request_handler.h"
#include "include/cef_life_span_handler.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <string>
#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include "viewportLogic/FrameDatas/FrameDatas.hpp"
#include "../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "SDL_Windows_states/SDL_State_Init.hpp"
#include "SDL_Windows_states/SDL_State_Maximized.hpp"
#include "SDL_Windows_states/SDL_State_Reduced.hpp"
#include "SDL_Windows_states/SDL_State_Scalingdown.hpp"
#include "SDL_Windows_states/SDL_State_ScalingUP.hpp"
#include "MouseStates/Mouse_Above_Workspace_State.hpp"
#include "MouseStates/Mouse_Outside_Workspace_State.hpp"

class ThreeDScreen;
class VKContext;
class VKScene;
class Camera;
class App_Border;
class Enable_UI_Debug_Tools;
class SDL_ApplicationWindow;
class PanelResizingLogic;

class AppRenderHandler : public CefRenderHandler
{
	public:
		explicit AppRenderHandler(SDL_ApplicationWindow* owner);
		void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
		void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
					const RectList& dirtyRects, const void* buffer, int width, int height) override;
	private:
		SDL_ApplicationWindow* owner_;
		IMPLEMENT_REFCOUNTING(AppRenderHandler);
};

class SIMILICefClient
	: public CefClient
	, public CefRequestHandler
	, public CefLifeSpanHandler
{
	public:
		explicit SIMILICefClient(SDL_ApplicationWindow* owner);

		// CefClient
		CefRefPtr<CefRenderHandler> GetRenderHandler() override;
		CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
		CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

		// CefRequestHandler
		CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
			CefRefPtr<CefBrowser> browser,
			CefRefPtr<CefFrame> frame,
			CefRefPtr<CefRequest> request,
			bool is_navigation,
			bool is_download,
			const CefString& request_initiator,
			bool& disable_default_handling) override;

		// CefLifeSpanHandler
		void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {}
		bool DoClose(CefRefPtr<CefBrowser> browser) override { return false; }
		void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {}

	private:
		SDL_ApplicationWindow* owner_;
		CefRefPtr<CefResourceRequestHandler> resource_request_handler_;
		IMPLEMENT_REFCOUNTING(SIMILICefClient);
};

enum WindowRenderState : int
{
	Init,
	Maximized,
	Reduced,
	ScaleUp,
	ScaleDown,
};

struct IFrameData;

namespace SIMILI
{
	namespace Frontend
	{
		class UIManager;
	}
}

class SDL_ApplicationWindow
{
	public:
		// === Lifecycle ===
		SDL_ApplicationWindow();
		~SDL_ApplicationWindow();
		
		bool create(const std::string& title, int width, int height, Uint32 flags = 0);
		void destroy();
		
		// === Window Management ===
		void setPosition(int x, int y);
		void setSize(int width, int height);
		void setTitle(const std::string& title);
		void show();
		void hide();
		void maximize();
		void restore();
		
		// === Window Properties ===
		void getPosition(int& x, int& y) const;
		void getSize(int& width, int& height) const;
		SDL_Rect getBounds() const;
		void getBorderOffsets(int& left, int& top, int& right, int& bottom) const;
		float getDpiScale() const;
		bool isMaximized() const;
		bool isVisible() const;
		WindowRenderState getWindowRenderState() const;
		bool isValid() const { return window_ != nullptr; }
		SDL_Window* getHandle() const { return window_; }

		// === Browser Access ===
		CefRefPtr<CefBrowser> getBrowser() const { return browser_; }
		CefRefPtr<CefRenderHandler> getRenderHandler() const { return cef_render_handler_; }
		CefRefPtr<SIMILICefClient> getCefClient();
		void setRenderPass(VkRenderPass renderPass);


		// === Component Registration ===
		void Set_UIHandler(void* handler);
		void setThreeDScreen(ThreeDScreen* screen);
		void setCamera(Camera* camera) { camera_ = camera; }
		void setVKContext(VKContext* context);
		void setVKScene(VKScene* scene) { vk_scene_ = scene; }
		void setVulkanPipelines(VulkanPipeline* pipelines);
		VulkanPipeline* getVulkanPipelines() const { return vulkan_pipelines_; }
		void setUIManager(SIMILI::Frontend::UIManager* manager) { ui_manager_ = manager; }
		void setPanelResizingLogic(PanelResizingLogic* logic) { panel_resizing_logic_ = logic; }
		void updateFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas);
		void onCEFPaint(CefRenderHandler::PaintElementType type, const void* buffer, int width, int height);

		SIMILI::Frontend::UIManager* getUIManager() const { return ui_manager_; }
		App_Border* getAppBorder() const { return app_border_; }
		
		// === SDL Window Reference Size ===
		void setSDLReferenceWindowSize(int width, int height) { reference_window_width_ = width; reference_window_height_ = height; }
		int getSDLReferenceWindowWidth() const { return reference_window_width_; }
		int getSDLReferenceWindowHeight() const { return reference_window_height_; }
		
		// === Event Handling ===
		void processEvents();
		void updateUIState();
		
		// === UI Initialization ===
		void initializeDefaultUIPanels();
		void forceCaptureIFramePositions();

		// === Rendering ===
		void renderFrame();
		void renderThreeDScreen(const std::map<std::string, IFrameData>& frameDataMap);
		void drawThreeDScreen();
		void preparePanels();
		void startSplitter();
		void activateDebugRender();
		void SetHTMLAdressToDraw(CefRefPtr<CefClient> client, const std::string& url, int width, int height);
		void requestBrowserRepaint();
		void setCurrentImageIndex(uint32_t index) { current_image_index_ = index; }
		
		// === Vulkan Lifecycle ===
		bool initializeVulkan();
		void cleanupVulkan();
		bool recreateSwapchain();
		
		// === Vulkan Object Access ===
		VkSurfaceKHR getVulkanSurface() const { return vk_surface_; }
		VkSwapchainKHR getSwapchain() const { return vk_swapchain_; }
		const std::vector<VkImage>& getSwapchainImages() const { return vk_swapchain_images_; }
		const std::vector<VkImageView>& getSwapchainImageViews() const { return vk_swapchain_image_views_; }
		const std::vector<VkFramebuffer>& getFramebuffers() const { return vk_framebuffers_; }
		VkRenderPass getRenderPass() const { return vk_render_pass_; }
		VkCommandPool getCommandPool() const { return vk_command_pool_; }
		const std::vector<VkCommandBuffer>& getCommandBuffers() const { return vk_command_buffers_; }
		uint32_t getCurrentImageIndex() const { return current_image_index_; }
		VkSemaphore getImageAvailableSemaphore() const { return vk_image_available_semaphore_; }
		VkSemaphore getRenderFinishedSemaphore() const { return vk_render_finished_semaphore_; }
		VkFence getInFlightFence() const { return vk_in_flight_fence_; }
		
	private:
		// === Window State ===
		SDL_Window* window_;
		bool is_maximized_;
		int last_x_;
		int last_y_;
		int last_width_;
		int last_height_;
		float dpi_scale_;
		int reference_window_width_;
		int reference_window_height_;
		
		// === UI Components ===
		void* ui_handler_;
		ThreeDScreen* threed_screen_;
		VKScene* vk_scene_;
		Camera* camera_;
		SIMILI::Frontend::FrameDatas* frame_datas_;
		SIMILI::Frontend::UIManager* ui_manager_;
		PanelResizingLogic* panel_resizing_logic_;
		App_Border* app_border_;
		Enable_UI_Debug_Tools* debug_tools_;

		// === CEF Client ===
		CefRefPtr<SIMILICefClient> simili_cef_client_;
		bool ui_manager_vulkan_initialized_;
		
		// === Graphics Components ===
		VKContext* vk_context_;
		VulkanPipeline* vulkan_pipelines_;
		CefRefPtr<CefBrowser> browser_;
		CefRefPtr<CefRenderHandler> cef_render_handler_;
		std::shared_ptr<VulkanPipeline::Pipeline> cef_shared_pipeline_;
		VkDescriptorSetLayout cef_descriptor_set_layout_;
		std::mutex render_mutex_;
		std::mutex queue_mutex_;
		std::vector<unsigned char> cef_paint_buffer_;
		int cef_paint_width_;
		int cef_paint_height_;
		VkImage cef_texture_image_;
		VkDeviceMemory cef_texture_memory_;
		VkImageView cef_texture_view_;
		VkSampler cef_texture_sampler_;
		int cef_texture_uploaded_width_;
		int cef_texture_uploaded_height_;
		
		// === Vulkan Surface & Swapchain ===
		VkSurfaceKHR vk_surface_;
		VkSwapchainKHR vk_swapchain_;
		std::vector<VkImage> vk_swapchain_images_;
		std::vector<VkImageView> vk_swapchain_image_views_;
		std::vector<VkFramebuffer> vk_framebuffers_;
		VkImage vk_depth_image_;
		VkDeviceMemory vk_depth_image_memory_;
		VkImageView vk_depth_image_view_;
		bool swapchain_needs_recreation_;
		
		// === Vulkan Render Resources ===
		VkRenderPass vk_render_pass_;
		VkCommandPool vk_command_pool_;
		std::vector<VkCommandBuffer> vk_command_buffers_;
		uint32_t current_image_index_;
		uint32_t current_frame_;
		static const int MAX_FRAMES_IN_FLIGHT = 2;
		
		// === Vulkan Synchronization ===
		std::vector<VkSemaphore> vk_image_available_semaphores_;
		std::vector<VkSemaphore> vk_render_finished_semaphores_;
		std::vector<VkFence> vk_in_flight_fences_;
		std::vector<VkFence> vk_image_fences_;
		VkSemaphore vk_image_available_semaphore_;
		VkSemaphore vk_render_finished_semaphore_;
		VkFence vk_in_flight_fence_;
		bool frame_acquisition_succeeded_;

		static int render_frame_count;

		// === Panel Rendering Data ===
		int prepared_drawable_width_;
		int prepared_drawable_height_;
		std::map<std::string, SIMILI::Frontend::IFrameScreenData> prepared_panel_frame_data_map_;
		bool prepared_skip_texture_rebuild_;
		bool borders_set_for_init_;
		bool pending_window_resize_sync_;

		// === Private Methods ===
		void updateDpiScale();
		void updateMaximizedState();
		bool createCEFPipeline();
		bool createCEFTextureSampler(VkDevice device);
		void uploadCEFPaintBuffer();
		void StateTransition(SDL_State* from, SDL_State* to);
		void updateMouseState(int mouseX, int mouseY);

		// === State Machine ===
		SDL_State_Init state_init_;
		SDL_State_Maximized state_maximized_;
		SDL_State_Reduced state_reduced_;
		SDL_State_ScalingDown state_scaling_down_;
		SDL_State_ScalingUp state_scaling_up_;
		SDL_State* current_state_;

		// === Mouse State Machine ===
		SIMILI::Input::Mouse_Above_Workspace_State mouse_state_above_workspace_;
		SIMILI::Input::Mouse_Outside_Workspace_State mouse_state_outside_workspace_;
		SIMILI::Input::Mouse_State* current_mouse_state_;

		// ===== Vulkan Resource Management ===== //
		bool createSwapchain();
		bool createRenderPass();
		bool createFramebuffers();
		bool createCommandPool();
		bool createCommandBuffers();
		bool createSyncObjects();
		void handleSwapchainRecreation();
		void swapchainSetup(int render_frame_count);
		bool renderPassViewportAndScissorSetup();
		bool finalizeAndSubmitCommandBuffer(VkCommandBuffer commandBuffer);
		void presentToScreen();
		void frameCounter();
};