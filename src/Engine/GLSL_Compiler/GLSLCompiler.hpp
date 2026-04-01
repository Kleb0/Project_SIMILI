#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace GLSLCompiler
{
	enum class ShaderType
	{
		Vertex,
		Fragment,
		Compute
	};

	// Compile GLSL source code to SPIR-V bytecode
	// Returns empty vector on failure (check error message via getLastError())
	std::vector<uint32_t> compileGLSL(const std::string& source, ShaderType type, const std::string& entryPoint = "main");

	// Get last compilation error message
	std::string getLastError();

	// Convert SPIR-V uint32_t vector to char vector (for VulkanPipeline compatibility)
	std::vector<char> spirvToBytes(const std::vector<uint32_t>& spirv);
}
