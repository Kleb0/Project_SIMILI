#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "../../SDL_ApplicationWindow.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

class VKContext;

namespace SIMILI 
{
    namespace Frontend 
    {

        class ContextualMenuAboveGUI;

        class ContextualMenuRenderHandler : public CefRenderHandler
        {

            public :
                explicit ContextualMenuRenderHandler(ContextualMenuAboveGUI* Owner);
                void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
                void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                            const RectList& dirtyRects, const void* buffer, int width, int height) override;

            private :

            ContextualMenuAboveGUI* owner_;
            IMPLEMENT_REFCOUNTING(ContextualMenuRenderHandler);
        };

        class ContextualMenuCefClient : public CefClient
        {
            public:
                explicit ContextualMenuCefClient(ContextualMenuAboveGUI* Owner);
                CefRefPtr<CefRenderHandler> GetRenderHandler() override;

            private:
                ContextualMenuAboveGUI* owner_;
                CefRefPtr<ContextualMenuRenderHandler> render_handler_;
                IMPLEMENT_REFCOUNTING(ContextualMenuCefClient);
        };

        class ContextualMenuAboveGUI
        {
            public:
                ContextualMenuAboveGUI();
                ~ContextualMenuAboveGUI();

                bool initializeVulkan(VKContext* vkContext, VulkanPipeline* pipelines,
                                    VkRenderPass renderPass, int widgetWidth, int widgetHeight);
                void loadURL(const std::string& url);
                void shutdown();

                void onPaint(const void* buffer, int width, int height);
                void uploadPaintBuffer();
                void makeFullyTransparent(std::vector<unsigned char>& pixelData, int width, int height);
                void draw(VkCommandBuffer commandBuffer, int wsX, int wsY, int drawableWidth, int drawableHeight);

                int getWidgetWidth()  const { return widget_width_;  }
                int getWidgetHeight() const { return widget_height_; }

                void SetPos(int x, int y) { menu_x_ = x; menu_y_ = y; }
                int getMenuX() const { return menu_x_; }
                int getMenuY() const { return menu_y_; }

            private:
                bool createTexture();
                bool createVertexBuffer();
                bool createDescriptorSet();
                bool createPipeline();
                void cleanupVulkanResources();
                void updateGeometry(int wsX, int wsY, int drawableWidth, int drawableHeight);
                uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

                VKContext* vk_context_;
                VulkanPipeline* vulkan_pipelines_;
                VkRenderPass vk_render_pass_;

                VkImage vk_texture_image_;
                VkDeviceMemory vk_texture_memory_;
                VkImageView vk_texture_view_;
                VkSampler vk_sampler_;

                VkDescriptorPool vk_descriptor_pool_;
                VkDescriptorSetLayout vk_descriptor_set_layout_;
                VkDescriptorSet vk_descriptor_set_;

                VkBuffer vk_vertex_buffer_;
                VkDeviceMemory vk_vertex_buffer_memory_;

                std::shared_ptr<VulkanPipeline::Pipeline> shared_pipeline_;

                CefRefPtr<ContextualMenuCefClient> cef_client_;
                CefRefPtr<CefBrowser> cef_browser_;

                mutable std::mutex paint_mutex_;
                std::vector<unsigned char> paint_buffer_;
                int paint_width_;
                int paint_height_;

                int  texture_uploaded_width_;
                int texture_uploaded_height_;
                VkImageView bound_texture_view_;
                VkSampler bound_sampler_;

                int  widget_width_;
                int  widget_height_;
                int  menu_x_;
                int  menu_y_;
                bool vulkan_initialized_;
                bool has_texture_;
                int last_ws_x_;
                int last_ws_y_;
                bool geometry_dirty_;
            };
    



    }

}


