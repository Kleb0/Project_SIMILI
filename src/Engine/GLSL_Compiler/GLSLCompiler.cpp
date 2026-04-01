#include "GLSLCompiler.hpp"
#include <shaderc/shaderc.hpp>
#include <iostream>

namespace GLSLCompiler
{
	static std::string s_lastError;

	std::vector<uint32_t> compileGLSL(const std::string& source, ShaderType type, const std::string& entryPoint)
	{
		s_lastError.clear();

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;

		// Set optimization level
		options.SetOptimizationLevel(shaderc_optimization_level_performance);

		// Set target environment to Vulkan 1.0
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_0);

		// Convert ShaderType to shaderc_shader_kind
		shaderc_shader_kind shaderKind;
		std::string shaderTypeStr;
		switch (type)
		{
			case ShaderType::Vertex:
				shaderKind = shaderc_vertex_shader;
				shaderTypeStr = "vertex";
				break;
			case ShaderType::Fragment:
				shaderKind = shaderc_fragment_shader;
				shaderTypeStr = "fragment";
				break;
			case ShaderType::Compute:
				shaderKind = shaderc_compute_shader;
				shaderTypeStr = "compute";
				break;
			default:
				s_lastError = "Unknown shader type";
				return {};
		}

		std::cout << "[GLSLCompiler] Compiling " << shaderTypeStr << " shader (" << source.size() << " bytes)" << std::endl;

		// Compile GLSL to SPIR-V
		shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
			source.c_str(),
			source.size(),
			shaderKind,
			"shader.glsl",
			entryPoint.c_str(),
			options
		);

		// Check compilation status
		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			s_lastError = module.GetErrorMessage();
			std::cerr << "[GLSLCompiler] Compilation failed: " << s_lastError << std::endl;
			return {};
		}

		// Extract SPIR-V bytecode
		std::vector<uint32_t> spirv(module.cbegin(), module.cend());

		std::cout << "[GLSLCompiler] Compilation successful - SPIR-V size: " << spirv.size() * 4 << " bytes" << std::endl;

		return spirv;
	}

	std::string getLastError()
	{
		return s_lastError;
	}

	std::vector<char> spirvToBytes(const std::vector<uint32_t>& spirv)
	{
		std::vector<char> bytes(spirv.size() * sizeof(uint32_t));
		std::memcpy(bytes.data(), spirv.data(), bytes.size());
		return bytes;
	}
}
