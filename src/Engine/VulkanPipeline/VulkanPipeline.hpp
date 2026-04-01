#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <map>

class VKContext;

// Vulkan Descriptor Set Manager for VulkanPipeline
class VulkanDescriptorManager
{
public:
    VulkanDescriptorManager();
    ~VulkanDescriptorManager();
    
    bool initialize(VKContext* context);
    void setContext(VKContext* context);
    void cleanup();
    
    // Descriptor set layout helpers
    VkDescriptorSetLayout createTextureDescriptorSetLayout();
    VkDescriptorSetLayout createUniformDescriptorSetLayout(uint32_t bufferSize);
    
    // Descriptor pool management
    VkDescriptorPool createDescriptorPool(uint32_t maxSets);
    
    // Descriptor set allocation
    VkDescriptorSet allocateDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout);
    
    // Update descriptor sets
    void updateTextureDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler sampler);
    void updateUniformBufferDescriptor(VkDescriptorSet descriptorSet, VkBuffer buffer, VkDeviceSize size);
    
private:
    VKContext* vk_context_;
    bool initialized_;
    
    // Cache for common layouts
    VkDescriptorSetLayout texture_layout_;
    VkDescriptorSetLayout uniform_layout_;
};

class VulkanPipeline
{
    public:

        struct PipelineConfig
        {
            std::string name; //< Unique pipeline identifier
            std::vector<char> vertexShaderCode;  //< SPIR-V vertex shader bytecode
            std::vector<char> fragmentShaderCode;  //< SPIR-V fragment shader bytecode
            VkRenderPass renderPass; //< Render pass for compatibility
            VkDescriptorSetLayout descriptorSetLayout;  //< Descriptor set layout (can be VK_NULL_HANDLE)
            bool enableBlending; //< Enable alpha blending
            VkPrimitiveTopology topology; //< Primitive topology (triangles, lines, etc.)
            uint32_t vertexBindingStride; //< Stride of vertex data in bytes
            std::vector<VkVertexInputAttributeDescription> vertexAttributes; //< Vertex attribute descriptions
            std::vector<VkPushConstantRange> pushConstantRanges; //< Push constant ranges (optional)
            
            PipelineConfig()
                : renderPass(VK_NULL_HANDLE)
                , descriptorSetLayout(VK_NULL_HANDLE)
                , enableBlending(true)
                , topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                , vertexBindingStride(0)
            {
            }
        };


        struct Pipeline
        {
            VkPipeline pipeline; //< Vulkan pipeline handle
            VkPipelineLayout layout; //< Associated pipeline layout
            VkShaderModule vertexShader; //< Vertex shader module
            VkShaderModule fragmentShader; //< Fragment shader module
            std::string name; //< Pipeline name for debugging
            
            Pipeline()
                : pipeline(VK_NULL_HANDLE)
                , layout(VK_NULL_HANDLE)
                , vertexShader(VK_NULL_HANDLE)
                , fragmentShader(VK_NULL_HANDLE)
            {
            }
        };

        VulkanPipeline();
        ~VulkanPipeline();

        bool initialize(VKContext* context);
        void setContext(VKContext* context);

        std::shared_ptr<Pipeline> getOrCreatePipeline(const PipelineConfig& config);

        std::shared_ptr<Pipeline> getPipeline(const std::string& name) const;

        void destroyPipeline(const std::string& name);

        void destroyAllPipelines();
        VKContext* getContext() const { return vk_context_; }

        bool isInitialized() const { return initialized_; }

    private:
        bool createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
        bool createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange>& pushConstantRanges, VkPipelineLayout* pipelineLayout);
        bool createGraphicsPipeline(const PipelineConfig& config, Pipeline* outPipeline);

        VKContext* vk_context_;
        bool initialized_;
        std::map<std::string, std::shared_ptr<Pipeline>> pipeline_cache_;
        std::unique_ptr<VulkanDescriptorManager> descriptor_manager_;
};
