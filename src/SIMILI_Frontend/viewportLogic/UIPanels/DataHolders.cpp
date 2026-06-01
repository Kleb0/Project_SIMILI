#include "DataHolders.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include "../../../Engine/GLSL_Compiler/GLSLCompiler.hpp"
#include "json.hpp"
#include <iostream>
#include <mutex>
#include <cstring>
#include <algorithm>

// 8x8 bitmap font for ASCII 0x20-0x7E (public domain)
// Each char: 8 bytes, each byte = 1 row, bit 0 = leftmost pixel
static const uint8_t kFont8x8[95][8] = {
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, // 0x20 space
    { 0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00 }, // 0x21 !
    { 0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00 }, // 0x22 "
    { 0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00 }, // 0x23 #
    { 0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00 }, // 0x24 $
    { 0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00 }, // 0x25 %
    { 0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00 }, // 0x26 &
    { 0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00 }, // 0x27 '
    { 0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00 }, // 0x28 (
    { 0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00 }, // 0x29 )
    { 0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00 }, // 0x2A *
    { 0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00 }, // 0x2B +
    { 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06 }, // 0x2C ,
    { 0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00 }, // 0x2D -
    { 0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00 }, // 0x2E .
    { 0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00 }, // 0x2F /
    { 0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00 }, // 0x30 0
    { 0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00 }, // 0x31 1
    { 0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00 }, // 0x32 2
    { 0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00 }, // 0x33 3
    { 0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00 }, // 0x34 4
    { 0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00 }, // 0x35 5
    { 0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00 }, // 0x36 6
    { 0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00 }, // 0x37 7
    { 0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00 }, // 0x38 8
    { 0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00 }, // 0x39 9
    { 0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00 }, // 0x3A :
    { 0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06 }, // 0x3B ;
    { 0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00 }, // 0x3C <
    { 0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00 }, // 0x3D =
    { 0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00 }, // 0x3E >
    { 0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00 }, // 0x3F ?
    { 0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00 }, // 0x40 @
    { 0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00 }, // 0x41 A
    { 0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00 }, // 0x42 B
    { 0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00 }, // 0x43 C
    { 0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00 }, // 0x44 D
    { 0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00 }, // 0x45 E
    { 0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00 }, // 0x46 F
    { 0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00 }, // 0x47 G
    { 0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00 }, // 0x48 H
    { 0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 }, // 0x49 I
    { 0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00 }, // 0x4A J
    { 0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00 }, // 0x4B K
    { 0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00 }, // 0x4C L
    { 0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00 }, // 0x4D M
    { 0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00 }, // 0x4E N
    { 0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00 }, // 0x4F O
    { 0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00 }, // 0x50 P
    { 0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00 }, // 0x51 Q
    { 0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00 }, // 0x52 R
    { 0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00 }, // 0x53 S
    { 0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 }, // 0x54 T
    { 0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00 }, // 0x55 U
    { 0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00 }, // 0x56 V
    { 0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00 }, // 0x57 W
    { 0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00 }, // 0x58 X
    { 0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00 }, // 0x59 Y
    { 0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00 }, // 0x5A Z
    { 0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00 }, // 0x5B [
    { 0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00 }, // 0x5C backslash
    { 0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00 }, // 0x5D ]
    { 0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00 }, // 0x5E ^
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF }, // 0x5F _
    { 0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00 }, // 0x60 `
    { 0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00 }, // 0x61 a
    { 0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00 }, // 0x62 b
    { 0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00 }, // 0x63 c
    { 0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00 }, // 0x64 d
    { 0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00 }, // 0x65 e
    { 0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00 }, // 0x66 f
    { 0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F }, // 0x67 g
    { 0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00 }, // 0x68 h
    { 0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00 }, // 0x69 i
    { 0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E }, // 0x6A j
    { 0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00 }, // 0x6B k
    { 0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00 }, // 0x6C l
    { 0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00 }, // 0x6D m
    { 0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00 }, // 0x6E n
    { 0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00 }, // 0x6F o
    { 0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F }, // 0x70 p
    { 0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78 }, // 0x71 q
    { 0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00 }, // 0x72 r
    { 0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00 }, // 0x73 s
    { 0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00 }, // 0x74 t
    { 0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00 }, // 0x75 u
    { 0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00 }, // 0x76 v
    { 0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00 }, // 0x77 w
    { 0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00 }, // 0x78 x
    { 0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F }, // 0x79 y
    { 0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00 }, // 0x7A z
    { 0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00 }, // 0x7B {
    { 0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00 }, // 0x7C |
    { 0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00 }, // 0x7D }
    { 0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00 }, // 0x7E ~
};

using json = nlohmann::json;

DataHolders* DataHolders::s_instance_ = nullptr;

DataHolders::DataHolders()
{
}

DataHolders* DataHolders::getInstance()
{
	return s_instance_;
}

void DataHolders::setInstance(DataHolders* instance)
{
	s_instance_ = instance;
}

bool DataHolders::receiveDataHolders(const std::string& jsonBody)
{
	json requestData = json::parse(jsonBody, nullptr, false);
	if (requestData.is_discarded())
	{
		return false;
	}

	if (!requestData.contains("panel") || !requestData.contains("dataHolders") || !requestData["dataHolders"].is_array())
	{
		return false;
	}

	std::string panelName = requestData["panel"];
	std::vector<DataHolderEntry> entries;

	for (const auto& holder : requestData["dataHolders"])
	{
		if (holder.contains("field"))
		{
			DataHolderEntry e;
			e.field = holder["field"].get<std::string>();
			e.relX = holder.value("x", 0);
			e.relY = holder.value("y", 0);
			e.width = holder.value("w", 50);
			e.height = holder.value("h", 20);
			entries.push_back(std::move(e));
		}
	}

	float vpFracW = requestData.value("vpFracW", 0.0f);
	float vpFracH = requestData.value("vpFracH", 0.0f);

	int entryCount = static_cast<int>(entries.size());
	{
		std::lock_guard<std::mutex> lock(mutex_);
		panel_fields_[panelName] = std::move(entries);
		if (vpFracW > 0.0f && vpFracH > 0.0f)
			panel_viewport_fracs_[panelName] = { vpFracW, vpFracH };

		auto& fields = panel_fields_[panelName];
		for (const auto& e : fields)
		{
			if (e.field == "selected_object")
			{
				if (field_values_[panelName].find("selected_object") == field_values_[panelName].end())
				{
					if (panelName == "object_inspector_panel")
						field_values_[panelName]["selected_object"] = "---- none ----";
					else
						field_values_[panelName]["selected_object"] = "No parent";
				}
				break;
			}
		}
	}

	std::cout << "[DataHolders] " << panelName << " -> Contains " << entryCount << " DataHolder(s)" << std::endl;

	return true;
}

int DataHolders::getCountForPanel(const std::string& panelName) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = panel_fields_.find(panelName);
	if (it != panel_fields_.end())
	{
		return static_cast<int>(it->second.size());
	}
	return 0;
}

void DataHolders::setFieldValue(const std::string& panelName, const std::string& field, const std::string& value)
{
	std::lock_guard<std::mutex> lock(mutex_);
	field_values_[panelName][field] = value;
}

std::string DataHolders::getFieldValue(const std::string& panelName, const std::string& field) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto pit = field_values_.find(panelName);
	if (pit == field_values_.end()) return "";
	auto fit = pit->second.find(field);
	if (fit == pit->second.end()) return "";
	return fit->second;
}

std::string DataHolders::getValuesAsJson(const std::string& panelName) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	json result = json::object();
	auto pit = field_values_.find(panelName);
	if (pit != field_values_.end())
	{
		for (const auto& kv : pit->second)
		{
			result[kv.first] = kv.second;
		}
	}
	return result.dump();
}

void DataHolders::initPlaceholderValues()
{
	const std::string panel = "object_inspector_panel";
	std::lock_guard<std::mutex> lock(mutex_);
	field_values_[panel]["selected_object"]  = "---- none ----";
	field_values_[panel]["position_x"] = "0.0";
	field_values_[panel]["position_y"] = "0.0";
	field_values_[panel]["position_z"] = "0.0";
	field_values_[panel]["rotation_x"] = "0.0";
	field_values_[panel]["rotation_y"] = "0.0";
	field_values_[panel]["rotation_z"] = "0.0";
	field_values_[panel]["scale_x"] = "0.0";
	field_values_[panel]["scale_y"] = "0.0";
	field_values_[panel]["scale_z"] = "0.0";
}

bool DataHolders::initializeVulkan(VKContext* context, VulkanPipeline* pipelines, VkRenderPass renderPass)
{
	vk_context_ = context;
	vulkan_pipelines_ = pipelines;
	vk_render_pass_ = renderPass;

	VkDevice device = vk_context_->getDevice();

	// --- Descriptor set layout (binding 0 = combined image sampler) ---
	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;
	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptor_set_layout_) != VK_SUCCESS)
	{
		std::cerr << "[DataHolders] Failed to create descriptor set layout" << std::endl;
		return false;
	}

	// --- Descriptor pool ---
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = static_cast<uint32_t>(kMaxOverlays);

	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.maxSets = static_cast<uint32_t>(kMaxOverlays);
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptor_pool_) != VK_SUCCESS)
	{
		std::cerr << "[DataHolders] Failed to create descriptor pool" << std::endl;
		return false;
	}

	// --- Text pipeline (pos + uv vertices, texture sampler) ---
	const std::string vertSrc = R"(
	#version 450
	layout(location = 0) in vec2 inPos;
	layout(location = 1) in vec2 inUV;
	layout(location = 0) out vec2 fragUV;
	void main() {
		gl_Position = vec4(inPos, 0.0, 1.0);
		fragUV = inUV;
	}
	)";

	const std::string fragSrc = R"(
	#version 450
	layout(set = 0, binding = 0) uniform sampler2D texSampler;
	layout(location = 0) in vec2 fragUV;
	layout(location = 0) out vec4 outColor;
	void main() {
		outColor = texture(texSampler, fragUV);
	}
	)";

	auto vertSpirv = GLSLCompiler::compileGLSL(vertSrc, GLSLCompiler::ShaderType::Vertex);
	if (vertSpirv.empty())
	{
		std::cerr << "[DataHolders] Vertex shader compile error: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	auto fragSpirv = GLSLCompiler::compileGLSL(fragSrc, GLSLCompiler::ShaderType::Fragment);
	if (fragSpirv.empty())
	{
		std::cerr << "[DataHolders] Fragment shader compile error: " << GLSLCompiler::getLastError() << std::endl;
		return false;
	}

	VulkanPipeline::PipelineConfig config;
	config.name = "dataholder_text_overlay_" + std::to_string(reinterpret_cast<uintptr_t>(renderPass));
	config.vertexShaderCode = GLSLCompiler::spirvToBytes(vertSpirv);
	config.fragmentShaderCode = GLSLCompiler::spirvToBytes(fragSpirv);
	config.renderPass = renderPass;
	config.descriptorSetLayout = descriptor_set_layout_;
	config.enableBlending = true;
	config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	config.vertexBindingStride = 4 * sizeof(float);

	VkVertexInputAttributeDescription posAttr{};
	posAttr.location = 0;
	posAttr.binding = 0;
	posAttr.format = VK_FORMAT_R32G32_SFLOAT;
	posAttr.offset = 0;
	config.vertexAttributes.push_back(posAttr);

	VkVertexInputAttributeDescription uvAttr{};
	uvAttr.location = 1;
	uvAttr.binding = 0;
	uvAttr.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttr.offset = 2 * sizeof(float);
	config.vertexAttributes.push_back(uvAttr);

	text_pipeline_ = vulkan_pipelines_->getOrCreatePipeline(config);
	if (!text_pipeline_ || text_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		std::cerr << "[DataHolders] Failed to create text pipeline" << std::endl;
		return false;
	}

	if (!createVertexBuffer())
	{
		return false;
	}

	vulkan_initialized_ = true;
	std::cout << "[DataHolders] Text overlay pipeline initialized" << std::endl;
	return true;
}

bool DataHolders::createVertexBuffer()
{
	// 6 vertices per quad, 4 floats per vertex (x, y, u, v)
	VkDeviceSize bufferSize = static_cast<VkDeviceSize>(kMaxOverlays * 6 * 4 * sizeof(float));

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = bufferSize;
	bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(vk_context_->getDevice(), &bufferInfo, nullptr, &vertex_buffer_) != VK_SUCCESS)
	{
		std::cerr << "[DataHolders] Failed to create overlay vertex buffer" << std::endl;
		return false;
	}

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(vk_context_->getDevice(), vertex_buffer_, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(vk_context_->getDevice(), &allocInfo, nullptr, &vertex_buffer_memory_) != VK_SUCCESS)
	{
		std::cerr << "[DataHolders] Failed to allocate overlay vertex buffer memory" << std::endl;
		vkDestroyBuffer(vk_context_->getDevice(), vertex_buffer_, nullptr);
		vertex_buffer_ = VK_NULL_HANDLE;
		return false;
	}

	vkBindBufferMemory(vk_context_->getDevice(), vertex_buffer_, vertex_buffer_memory_, 0);
	return true;
}

uint32_t DataHolders::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vk_context_->getPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	return 0;
}

void DataHolders::drawForPanel(
	const std::string& panelName,
	int panelDrawX, int panelDrawY, int panelDrawW, int panelDrawH,
	int logicalW, int logicalH,
	VkCommandBuffer cmdBuffer, int drawableW, int drawableH,
	VKContext* vkContext, VulkanPipeline* vkPipelines, VkRenderPass renderPass)
{
	if (!vkContext || !vkPipelines || renderPass == VK_NULL_HANDLE) return;
	if (cmdBuffer == VK_NULL_HANDLE) return;
	if (drawableW <= 0 || drawableH <= 0) return;
	if (panelDrawW <= 0 || panelDrawH <= 0) return;

	if (!vulkan_initialized_)
	{
		if (!initializeVulkan(vkContext, vkPipelines, renderPass)) return;
	}
	else if (vk_render_pass_ != renderPass)
	{
		// RenderPass was recreated (e.g. window resize): rebuild pipeline
		cleanupVulkan();
		if (!initializeVulkan(vkContext, vkPipelines, renderPass)) return;
	}
	else if (!text_pipeline_ || text_pipeline_->pipeline == VK_NULL_HANDLE)
	{
		cleanupVulkan();
		if (!initializeVulkan(vkContext, vkPipelines, renderPass)) return;
	}

	std::vector<DataHolderEntry> entries;
	std::map<std::string, std::string> values;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto it = panel_fields_.find(panelName);
		if (it == panel_fields_.end() || it->second.empty()) return;
		entries = it->second;
		auto vit = field_values_.find(panelName);
		if (vit != field_values_.end())
			values = vit->second;
	}

	int count = std::min(static_cast<int>(entries.size()), kMaxOverlays);
	float scaleX = (logicalW > 0) ? static_cast<float>(panelDrawW) / logicalW : 1.0f;
	float scaleY = (logicalH > 0) ? static_cast<float>(panelDrawH) / logicalH : 1.0f;

	float vpFracW = 1.0f, vpFracH = 1.0f;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto vit = panel_viewport_fracs_.find(panelName);
		if (vit != panel_viewport_fracs_.end())
		{
			vpFracW = vit->second.first;
			vpFracH = vit->second.second;
		}
	}

	int scissorX = std::max(0, panelDrawX);
	int scissorY = std::max(0, panelDrawY);
	int scissorW = std::max(1, std::min(static_cast<int>(vpFracW * panelDrawW), drawableW - scissorX));
	int scissorH = std::max(1, std::min(static_cast<int>(vpFracH * panelDrawH), drawableH - scissorY));

	std::vector<float> vertices;
	vertices.reserve(count * 6 * 4);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, text_pipeline_->pipeline);

	VkRect2D scissor{};
	scissor.offset = { scissorX, scissorY };
	scissor.extent = { static_cast<uint32_t>(scissorW), static_cast<uint32_t>(scissorH) };
	vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

	for (int i = 0; i < count; i++)
	{
		const auto& e = entries[i];

		std::string text;
		auto vit2 = values.find(e.field);
		if (vit2 != values.end())
			text = vit2->second;
		if (text.empty())
			continue;

		int texW = std::max(1, e.width);
		int texH = std::max(1, e.height);

		TextOverlay& ov = text_overlays_[i];
		if (ov.lastText != text || ov.lastWidth != texW || ov.lastHeight != texH)
		{
			cleanupTextOverlay(ov);
			uploadTextTexture(text, texW, texH, ov);
			ov.lastText = text;
			ov.lastWidth = texW;
			ov.lastHeight = texH;
		}

		if (ov.descriptorSet == VK_NULL_HANDLE)
			continue;

		float ax = panelDrawX + e.relX * scaleX;
		float ay = panelDrawY + e.relY * scaleY;
		float bx = ax + e.width  * scaleX;
		float by = ay + e.height * scaleY;

		float ndcX0 = 2.0f * ax / drawableW - 1.0f;
		float ndcY0 = 2.0f * ay / drawableH - 1.0f;
		float ndcX1 = 2.0f * bx / drawableW - 1.0f;
		float ndcY1 = 2.0f * by / drawableH - 1.0f;

		float verts[24] = {
			ndcX0, ndcY0,  0.0f, 0.0f,
			ndcX1, ndcY0,  1.0f, 0.0f,
			ndcX0, ndcY1,  0.0f, 1.0f,
			ndcX1, ndcY0,  1.0f, 0.0f,
			ndcX1, ndcY1,  1.0f, 1.0f,
			ndcX0, ndcY1,  0.0f, 1.0f,
		};

		void* mapped;
		vkMapMemory(vkContext->getDevice(), vertex_buffer_memory_,
			i * 6 * 4 * sizeof(float), 6 * 4 * sizeof(float), 0, &mapped);
		std::memcpy(mapped, verts, sizeof(verts));
		vkUnmapMemory(vkContext->getDevice(), vertex_buffer_memory_);

		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			text_pipeline_->layout, 0, 1, &ov.descriptorSet, 0, nullptr);

		VkBuffer buf = vertex_buffer_;
		VkDeviceSize offset = static_cast<VkDeviceSize>(i * 6 * 4 * sizeof(float));
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &buf, &offset);
		vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
	}

	VkRect2D fullScissor{};
	fullScissor.offset = { 0, 0 };
	fullScissor.extent = { static_cast<uint32_t>(drawableW), static_cast<uint32_t>(drawableH) };
	vkCmdSetScissor(cmdBuffer, 0, 1, &fullScissor);
}

std::vector<uint8_t> DataHolders::rasterizeText(const std::string& text, int width, int height)
{
	std::vector<uint8_t> pixels(width * height * 4, 0);

	const int charW = 8;
	const int charH = 8;
	const int scale = 1;
	const int offsetY = (height - charH * scale) / 2;
	int cursorX = 5;

	for (char ch : text)
	{
		if (cursorX + charW * scale > width) break;
		int idx = static_cast<unsigned char>(ch) - 0x20;
		if (idx < 0 || idx >= 95)
		{
			cursorX += charW * scale;
			continue;
		}
		const uint8_t* glyph = kFont8x8[idx];
		for (int row = 0; row < charH; row++)
		{
			for (int col = 0; col < charW; col++)
			{
				bool lit = (glyph[row] >> col) & 1;
				if (!lit) continue;
				for (int sy = 0; sy < scale; sy++)
				{
					for (int sx = 0; sx < scale; sx++)
					{
						int px = cursorX + col * scale + sx;
						int py = offsetY + row * scale + sy;
						if (px >= width || py >= height) continue;
						int pidx = (py * width + px) * 4;
						pixels[pidx + 0] = 255;
						pixels[pidx + 1] = 255;
						pixels[pidx + 2] = 255;
						pixels[pidx + 3] = 255;
					}
				}
			}
		}
		cursorX += charW * scale;
	}
	return pixels;
}

bool DataHolders::uploadTextTexture(const std::string& text, int width, int height, TextOverlay& overlay)
{
	VkDevice device = vk_context_->getDevice();

	auto pixels = rasterizeText(text, width, height);

	VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imgInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	imgInfo.mipLevels = 1;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imgInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(device, &imgInfo, nullptr, &overlay.image) != VK_SUCCESS)
		return false;

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(device, overlay.image, &memReq);

	VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &overlay.memory) != VK_SUCCESS)
	{
		vkDestroyImage(device, overlay.image, nullptr);
		overlay.image = VK_NULL_HANDLE;
		return false;
	}
	vkBindImageMemory(device, overlay.image, overlay.memory, 0);

	VkImageSubresource subRes{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
	VkSubresourceLayout layout;
	vkGetImageSubresourceLayout(device, overlay.image, &subRes, &layout);

	void* mapped;
	vkMapMemory(device, overlay.memory, 0, memReq.size, 0, &mapped);
	uint8_t* dst = static_cast<uint8_t*>(mapped) + layout.offset;
	for (int row = 0; row < height; row++)
	{
		std::memcpy(dst + row * layout.rowPitch,
			pixels.data() + row * width * 4,
			width * 4);
	}
	vkUnmapMemory(device, overlay.memory);

	VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.queueFamilyIndex = vk_context_->getGraphicsQueueFamily();
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	VkCommandPool pool = VK_NULL_HANDLE;
	vkCreateCommandPool(device, &poolInfo, nullptr, &pool);

	VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmdAlloc.commandPool = pool;
	cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdAlloc.commandBufferCount = 1;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(device, &cmdAlloc, &cmd);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = overlay.image;
	barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmd,
		VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);

	vkEndCommandBuffer(cmd);
	VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	vkQueueSubmit(vk_context_->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vk_context_->getGraphicsQueue());
	vkDestroyCommandPool(device, pool, nullptr);

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = overlay.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	if (vkCreateImageView(device, &viewInfo, nullptr, &overlay.view) != VK_SUCCESS)
		return false;

	VkSamplerCreateInfo sampInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampInfo.magFilter = VK_FILTER_NEAREST;
	sampInfo.minFilter = VK_FILTER_NEAREST;
	sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	if (vkCreateSampler(device, &sampInfo, nullptr, &overlay.sampler) != VK_SUCCESS)
		return false;

	VkDescriptorSetAllocateInfo dsAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	dsAlloc.descriptorPool = descriptor_pool_;
	dsAlloc.descriptorSetCount = 1;
	dsAlloc.pSetLayouts = &descriptor_set_layout_;
	if (vkAllocateDescriptorSets(device, &dsAlloc, &overlay.descriptorSet) != VK_SUCCESS)
		return false;

	VkDescriptorImageInfo imgDescInfo{};
	imgDescInfo.sampler = overlay.sampler;
	imgDescInfo.imageView = overlay.view;
	imgDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	write.dstSet = overlay.descriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imgDescInfo;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

	return true;
}

void DataHolders::cleanupTextOverlay(TextOverlay& overlay)
{
	if (!vk_context_) return;
	VkDevice device = vk_context_->getDevice();
	vkQueueWaitIdle(vk_context_->getGraphicsQueue());
	if (overlay.sampler   != VK_NULL_HANDLE) { vkDestroySampler(device, overlay.sampler, nullptr);   overlay.sampler   = VK_NULL_HANDLE; }
	if (overlay.view      != VK_NULL_HANDLE) { vkDestroyImageView(device, overlay.view, nullptr);    overlay.view      = VK_NULL_HANDLE; }
	if (overlay.image     != VK_NULL_HANDLE) { vkDestroyImage(device, overlay.image, nullptr);       overlay.image     = VK_NULL_HANDLE; }
	if (overlay.memory    != VK_NULL_HANDLE) { vkFreeMemory(device, overlay.memory, nullptr);        overlay.memory    = VK_NULL_HANDLE; }
	overlay.descriptorSet = VK_NULL_HANDLE;
	overlay.lastText.clear();
}

void DataHolders::cleanupVulkan()
{
	if (vk_context_)
	{
		vkQueueWaitIdle(vk_context_->getGraphicsQueue());
		for (int i = 0; i < kMaxOverlays; i++)
			cleanupTextOverlay(text_overlays_[i]);

		VkDevice device = vk_context_->getDevice();
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
	}
	text_pipeline_ = nullptr;
	vulkan_initialized_ = false;
}
