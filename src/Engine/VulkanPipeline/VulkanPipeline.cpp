#include "VulkanPipeline.hpp"
#include "../VulkanScene/VKcontext.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

// ==============================================================================
// VulkanDescriptorManager Implementation
// ==============================================================================

VulkanDescriptorManager::VulkanDescriptorManager()
    : vk_context_(nullptr)
    , initialized_(false)
    , texture_layout_(VK_NULL_HANDLE)
    , uniform_layout_(VK_NULL_HANDLE)
{
}

VulkanDescriptorManager::~VulkanDescriptorManager()
{
    cleanup();
}

bool VulkanDescriptorManager::initialize(VKContext* context)
{
    if (!context)
    {
        std::cerr << "[VulkanDescriptorManager] ERROR: VKContext is NULL" << std::endl;
        return false;
    }

    setContext(context);
    
    // Pre-create common descriptor set layouts
    texture_layout_ = createTextureDescriptorSetLayout();
    if (texture_layout_ == VK_NULL_HANDLE)
    {
        std::cerr << "[VulkanDescriptorManager] Failed to create texture descriptor layout" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "[VulkanDescriptorManager] Initialized successfully" << std::endl;
    return true;
}

void VulkanDescriptorManager::setContext(VKContext* context)
{
    vk_context_ = context;
}

void VulkanDescriptorManager::cleanup()
{
    if (!vk_context_ || !initialized_)
        return;
        
    VkDevice device = vk_context_->getDevice();
    
    if (texture_layout_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, texture_layout_, nullptr);
        texture_layout_ = VK_NULL_HANDLE;
    }
    
    if (uniform_layout_ != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, uniform_layout_, nullptr);
        uniform_layout_ = VK_NULL_HANDLE;
    }
    
    initialized_ = false;
    std::cout << "[VulkanDescriptorManager] Cleaned up" << std::endl;
}

VkDescriptorSetLayout VulkanDescriptorManager::createTextureDescriptorSetLayout()
{
    if (!vk_context_)
        return VK_NULL_HANDLE;
        
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerBinding.pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerBinding;
    
    VkDescriptorSetLayout layout;
    VkResult result = vkCreateDescriptorSetLayout(vk_context_->getDevice(), &layoutInfo, nullptr, &layout);
    
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanDescriptorManager] Failed to create texture descriptor set layout (VkResult=" << result << ")" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    std::cout << "[VulkanDescriptorManager] Created texture descriptor set layout: " << layout << std::endl;
    return layout;
}

VkDescriptorSetLayout VulkanDescriptorManager::createUniformDescriptorSetLayout(uint32_t bufferSize)
{
    if (!vk_context_)
        return VK_NULL_HANDLE;
        
    VkDescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.binding = 0;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    uniformBinding.pImmutableSamplers = nullptr;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uniformBinding;
    
    VkDescriptorSetLayout layout;
    VkResult result = vkCreateDescriptorSetLayout(vk_context_->getDevice(), &layoutInfo, nullptr, &layout);
    
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanDescriptorManager] Failed to create uniform descriptor set layout (VkResult=" << result << ")" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    std::cout << "[VulkanDescriptorManager] Created uniform descriptor set layout: " << layout << " (size=" << bufferSize << ")" << std::endl;
    return layout;
}

VkDescriptorPool VulkanDescriptorManager::createDescriptorPool(uint32_t maxSets)
{
    if (!vk_context_)
        return VK_NULL_HANDLE;
        
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets}
    };
    
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = maxSets * 2;  // Allow for both types
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    
    VkDescriptorPool pool;
    VkResult result = vkCreateDescriptorPool(vk_context_->getDevice(), &poolInfo, nullptr, &pool);
    
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanDescriptorManager] Failed to create descriptor pool (VkResult=" << result << ")" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    std::cout << "[VulkanDescriptorManager] Created descriptor pool: " << pool << " (maxSets=" << maxSets << ")" << std::endl;
    return pool;
}

VkDescriptorSet VulkanDescriptorManager::allocateDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    if (!vk_context_ || pool == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
        return VK_NULL_HANDLE;
        
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    
    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(vk_context_->getDevice(), &allocInfo, &descriptorSet);
    
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanDescriptorManager] Failed to allocate descriptor set (VkResult=" << result << ")" << std::endl;
        return VK_NULL_HANDLE;
    }
    
    return descriptorSet;
}

void VulkanDescriptorManager::updateTextureDescriptor(VkDescriptorSet descriptorSet, VkImageView imageView, VkSampler sampler)
{
    if (!vk_context_ || descriptorSet == VK_NULL_HANDLE)
        return;
        
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;
    
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;
    
    vkUpdateDescriptorSets(vk_context_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

void VulkanDescriptorManager::updateUniformBufferDescriptor(VkDescriptorSet descriptorSet, VkBuffer buffer, VkDeviceSize size)
{
    if (!vk_context_ || descriptorSet == VK_NULL_HANDLE)
        return;
        
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = size;
    
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    
    vkUpdateDescriptorSets(vk_context_->getDevice(), 1, &descriptorWrite, 0, nullptr);
}

// ==============================================================================
// VulkanPipeline Implementation
// ==============================================================================

VulkanPipeline::VulkanPipeline()
    : vk_context_(nullptr)
    , initialized_(false)
    , descriptor_manager_(std::make_unique<VulkanDescriptorManager>())
{
}

VulkanPipeline::~VulkanPipeline()
{
    destroyAllPipelines();
}

bool VulkanPipeline::initialize(VKContext* context)
{
    if (!context)
    {
        std::cerr << "[VulkanPipeline] ERROR: VKContext is NULL" << std::endl;
        return false;
    }

    setContext(context);
    
    // Initialize descriptor manager
    if (!descriptor_manager_->initialize(context))
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to initialize descriptor manager" << std::endl;
        return false;
    }
    
    initialized_ = true;
    std::cout << "[VulkanPipeline] Initialized successfully with descriptor manager" << std::endl;
    return true;
}

void VulkanPipeline::setContext(VKContext* context)
{
    vk_context_ = context;
    if (descriptor_manager_)
    {
        descriptor_manager_->setContext(context);
    }
}

std::shared_ptr<VulkanPipeline::Pipeline> VulkanPipeline::getOrCreatePipeline(const PipelineConfig& config)
{
    if (!initialized_ || !vk_context_)
    {
        std::cerr << "[VulkanPipeline] ERROR: Not initialized" << std::endl;
        return nullptr;
    }

    // Check cache first
    auto it = pipeline_cache_.find(config.name);
    if (it != pipeline_cache_.end())
    {
        std::cout << "[VulkanPipeline] Using cached pipeline: " << config.name << std::endl;
        return it->second;
    }

    // Create new pipeline
    std::cout << "[VulkanPipeline] Creating new pipeline: " << config.name << std::endl;
    
    auto pipeline = std::make_shared<Pipeline>();
    pipeline->name = config.name;

    if (!createGraphicsPipeline(config, pipeline.get()))
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to create pipeline: " << config.name << std::endl;
        return nullptr;
    }

    pipeline_cache_[config.name] = pipeline;
    std::cout << "[VulkanPipeline] Pipeline created and cached: " << config.name 
              << " (VkPipeline=" << pipeline->pipeline << ")" << std::endl;
    
    return pipeline;
}

std::shared_ptr<VulkanPipeline::Pipeline> VulkanPipeline::getPipeline(const std::string& name) const
{
    auto it = pipeline_cache_.find(name);
    if (it != pipeline_cache_.end())
    {
        return it->second;
    }
    return nullptr;
}

void VulkanPipeline::destroyPipeline(const std::string& name)
{
    auto it = pipeline_cache_.find(name);
    if (it == pipeline_cache_.end())
    {
        return;
    }

    std::cout << "[VulkanPipeline] Destroying pipeline: " << name << std::endl;
    
    auto& pipeline = it->second;
    VkDevice device = vk_context_->getDevice();

    if (pipeline->pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline->pipeline, nullptr);
    }

    if (pipeline->layout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipeline->layout, nullptr);
    }

    if (pipeline->vertexShader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, pipeline->vertexShader, nullptr);
    }

    if (pipeline->fragmentShader != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, pipeline->fragmentShader, nullptr);
    }

    pipeline_cache_.erase(it);
}

void VulkanPipeline::destroyAllPipelines()
{
    if (!vk_context_)
    {
        pipeline_cache_.clear();
        return;
    }

    std::cout << "[VulkanPipeline] Destroying all pipelines (" << pipeline_cache_.size() << ")" << std::endl;
    
    VkDevice device = vk_context_->getDevice();
    vkDeviceWaitIdle(device);

    for (auto& pair : pipeline_cache_)
    {
        auto& pipeline = pair.second;

        if (pipeline->pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, pipeline->pipeline, nullptr);
            pipeline->pipeline = VK_NULL_HANDLE;  // null so shared_ptr holders can detect invalidation
        }

        if (pipeline->layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, pipeline->layout, nullptr);
            pipeline->layout = VK_NULL_HANDLE;
        }

        if (pipeline->vertexShader != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, pipeline->vertexShader, nullptr);
            pipeline->vertexShader = VK_NULL_HANDLE;
        }

        if (pipeline->fragmentShader != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(device, pipeline->fragmentShader, nullptr);
            pipeline->fragmentShader = VK_NULL_HANDLE;
        }
    }

    pipeline_cache_.clear();
    initialized_ = false;
}

bool VulkanPipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule)
{
    if (code.empty())
    {
        std::cerr << "[VulkanPipeline] ERROR: Shader bytecode is empty" << std::endl;
        return false;
    }

    if ((code.size() % sizeof(uint32_t)) != 0)
    {
        std::cerr << "[VulkanPipeline] ERROR: Shader bytecode size is not 4-byte aligned: " << code.size() << std::endl;
        return false;
    }

    std::vector<uint32_t> alignedCode(code.size() / sizeof(uint32_t));
    std::memcpy(alignedCode.data(), code.data(), code.size());

    if (alignedCode.front() != 0x07230203)
    {
        std::cerr << "[VulkanPipeline] ERROR: Invalid SPIR-V magic number: 0x" << std::hex << alignedCode.front() << std::dec << std::endl;
        return false;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = alignedCode.data();

    VkResult result = vkCreateShaderModule(vk_context_->getDevice(), &createInfo, nullptr, shaderModule);
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to create shader module (VkResult=" << result << ")" << std::endl;
        return false;
    }

    return true;
}

bool VulkanPipeline::createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout, const std::vector<VkPushConstantRange>& pushConstantRanges, VkPipelineLayout* pipelineLayout)
{
    std::cout << "[VulkanPipeline::createPipelineLayout] Creating layout with:" << std::endl;
    std::cout << "  DescriptorSetLayout: " << descriptorSetLayout << std::endl;
    std::cout << "  PushConstantRanges: " << pushConstantRanges.size() << std::endl;
    
    if (!pushConstantRanges.empty())
    {
        for (size_t i = 0; i < pushConstantRanges.size(); ++i)
        {
            std::cout << "  Range[" << i << "]: stageFlags=" << pushConstantRanges[i].stageFlags 
                      << " offset=" << pushConstantRanges[i].offset 
                      << " size=" << pushConstantRanges[i].size << std::endl;
        }
    }
    
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &descriptorSetLayout;
    }
    else
    {
        layoutInfo.setLayoutCount = 0;
        layoutInfo.pSetLayouts = nullptr;
    }
    
    if (!pushConstantRanges.empty())
    {
        layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        layoutInfo.pPushConstantRanges = pushConstantRanges.data();
    }
    else
    {
        layoutInfo.pushConstantRangeCount = 0;
        layoutInfo.pPushConstantRanges = nullptr;
    }

    VkResult result = vkCreatePipelineLayout(vk_context_->getDevice(), &layoutInfo, nullptr, pipelineLayout);
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to create pipeline layout (VkResult=" << result << ")" << std::endl;
        return false;
    }

    return true;
}

bool VulkanPipeline::createGraphicsPipeline(const PipelineConfig& config, Pipeline* outPipeline)
{
    std::cout << "[VulkanPipeline::createGraphicsPipeline] Starting for: " << config.name << std::endl;
    std::cout << "  RenderPass: " << config.renderPass << std::endl;
    std::cout << "  DescriptorSetLayout: " << config.descriptorSetLayout << std::endl;
    std::cout << "  VertexShaderSize: " << config.vertexShaderCode.size() << std::endl;
    std::cout << "  FragmentShaderSize: " << config.fragmentShaderCode.size() << std::endl;
    std::cout << "  Topology: " << config.topology << std::endl;
    std::cout << "  VertexBindingStride: " << config.vertexBindingStride << std::endl;
    std::cout << "  VertexAttributes: " << config.vertexAttributes.size() << std::endl;
    
    if (config.renderPass == VK_NULL_HANDLE)
    {
        std::cerr << "[VulkanPipeline] ERROR: Render pass is NULL" << std::endl;
        return false;
    }

    // Create shader modules
    std::cout << "[VulkanPipeline] Creating vertex shader module..." << std::endl;
    if (!createShaderModule(config.vertexShaderCode, &outPipeline->vertexShader))
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to create vertex shader module" << std::endl;
        return false;
    }
    std::cout << "[VulkanPipeline] Vertex shader created: " << outPipeline->vertexShader << std::endl;

    std::cout << "[VulkanPipeline] Creating fragment shader module..." << std::endl;
    if (!createShaderModule(config.fragmentShaderCode, &outPipeline->fragmentShader))
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to create fragment shader module" << std::endl;
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->vertexShader, nullptr);
        return false;
    }
    std::cout << "[VulkanPipeline] Fragment shader created: " << outPipeline->fragmentShader << std::endl;

    // Shader stages
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = outPipeline->vertexShader;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = outPipeline->fragmentShader;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Vertex input
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = config.vertexBindingStride;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    
    if (config.vertexBindingStride > 0)
    {
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertexAttributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = config.vertexAttributes.data();
        
        std::cout << "[VulkanPipeline] Vertex input configured:" << std::endl;
        std::cout << "  bindingDescription.stride: " << bindingDescription.stride << std::endl;
        std::cout << "  vertexAttributeCount: " << config.vertexAttributes.size() << std::endl;
        for (size_t i = 0; i < config.vertexAttributes.size(); ++i)
        {
            std::cout << "  attr[" << i << "]: binding=" << config.vertexAttributes[i].binding 
                      << " location=" << config.vertexAttributes[i].location
                      << " format=" << config.vertexAttributes[i].format
                      << " offset=" << config.vertexAttributes[i].offset << std::endl;
        }
    }
    else
    {
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
    }

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = config.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor (dynamic) - dummy values required by some drivers
    VkViewport dummyViewport = {};
    dummyViewport.x = 0.0f;
    dummyViewport.y = 0.0f;
    dummyViewport.width = 1920.0f;
    dummyViewport.height = 1080.0f;
    dummyViewport.minDepth = 0.0f;
    dummyViewport.maxDepth = 1.0f;

    VkRect2D dummyScissor = {};
    dummyScissor.offset = {0, 0};
    dummyScissor.extent = {1920, 1080};

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &dummyViewport;  // Required even with dynamic state on some drivers
    viewportState.scissorCount = 1;
    viewportState.pScissors = &dummyScissor;    // Required even with dynamic state on some drivers

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling for UI
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    if (config.enableBlending)
    {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    else
    {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Depth/stencil state (disabled but explicit)
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    bool isMeshPipeline = (config.name.rfind("mesh_", 0) == 0);
    depthStencil.depthTestEnable = isMeshPipeline ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = (isMeshPipeline && config.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST) ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = isMeshPipeline ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_ALWAYS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;

    // Dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Create pipeline layout
    std::cout << "[VulkanPipeline] Creating pipeline layout..." << std::endl;
    if (!createPipelineLayout(config.descriptorSetLayout, config.pushConstantRanges, &outPipeline->layout))
    {
        std::cerr << "[VulkanPipeline] ERROR: Failed to create pipeline layout" << std::endl;
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->vertexShader, nullptr);
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->fragmentShader, nullptr);
        return false;
    }
    std::cout << "[VulkanPipeline] Pipeline layout created: " << outPipeline->layout << std::endl;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = outPipeline->layout;
    pipelineInfo.renderPass = config.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    std::cout << "[VulkanPipeline] Creating graphics pipeline..." << std::endl;
    
    VkPipeline tempPipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(
        vk_context_->getDevice(),
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &tempPipeline
    );
    
    // EXPLICIT CHECK: Driver bug - VK_SUCCESS but NULL handle
    if (result == VK_SUCCESS && tempPipeline == VK_NULL_HANDLE)
    {
        std::cerr << "[VulkanPipeline] ERROR: Driver returned VK_SUCCESS but pipeline handle is NULL" << std::endl;
        std::cerr << "[VulkanPipeline] This is a driver bug or validation layer issue" << std::endl;
        std::cerr << "[VulkanPipeline] Failed to create pipeline: " << config.name << std::endl;
        vkDestroyPipelineLayout(vk_context_->getDevice(), outPipeline->layout, nullptr);
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->vertexShader, nullptr);
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->fragmentShader, nullptr);
        outPipeline->pipeline = VK_NULL_HANDLE;
        return false;
    }
    
    if (result != VK_SUCCESS)
    {
        std::cerr << "[VulkanPipeline] ERROR: vkCreateGraphicsPipelines failed with VkResult=" << result << std::endl;
        std::cerr << "[VulkanPipeline] Failed to create pipeline: " << config.name << std::endl;
        vkDestroyPipelineLayout(vk_context_->getDevice(), outPipeline->layout, nullptr);
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->vertexShader, nullptr);
        vkDestroyShaderModule(vk_context_->getDevice(), outPipeline->fragmentShader, nullptr);
        outPipeline->pipeline = VK_NULL_HANDLE;
        return false;
    }
    
    outPipeline->pipeline = tempPipeline;
    std::cout << "[VulkanPipeline] Graphics pipeline created successfully: " << config.name 
              << " (pipeline=" << outPipeline->pipeline << " layout=" << outPipeline->layout << ")" << std::endl;
    
    return true;
}
