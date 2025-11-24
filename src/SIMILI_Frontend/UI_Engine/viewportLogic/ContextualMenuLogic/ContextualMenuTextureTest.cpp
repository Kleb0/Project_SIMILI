#include "ContextualMenuTextureTest.hpp"
#include <iostream>
#include <vector>

// Vertex shader for rendering a textured quad
const char* vertex_shader_source = R"(
#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
	gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
	TexCoord = aTexCoord;
}
)";

// Fragment shader for rendering the texture
const char* fragment_shader_source = R"(
#version 460 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;

void main()
{
	FragColor = texture(texture1, TexCoord);
}
)";

ContextualMenuTextureTest::ContextualMenuTextureTest()
	: texture_id_(0)
	, shader_program_(0)
	, vao_(0)
	, vbo_(0)
	, width_(0)
	, height_(0)
	, initialized_(false)
	, visible_(false)
{
}

ContextualMenuTextureTest::~ContextualMenuTextureTest()
{
	cleanup();
}

void ContextualMenuTextureTest::initialize(int width, int height)
{
	if (initialized_) {
		return;
	}
	
	width_ = width;
	height_ = height;
	
	createTexture();
	createShaderProgram();
	createQuad();
	
	initialized_ = true;
	
	std::cout << "[ContextualMenuTextureTest] Initialized with size " << width << "x" << height << std::endl;
}

void ContextualMenuTextureTest::createTexture()
{
	// Generate texture
	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	
	// Set texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	// Create red texture data (RGBA format)
	std::vector<unsigned char> red_data(width_ * height_ * 4);
	for (int i = 0; i < width_ * height_; ++i) {
		red_data[i * 4 + 0] = 255;
		red_data[i * 4 + 1] = 0;
		red_data[i * 4 + 2] = 0; 
		red_data[i * 4 + 3] = 255;  
	}
	
	// Upload texture data
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, red_data.data());
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	std::cout << "[ContextualMenuTextureTest] Red texture created (ID: " << texture_id_ << ")" << std::endl;
}

void ContextualMenuTextureTest::createShaderProgram()
{
	// Compile vertex shader
	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
	glCompileShader(vertex_shader);
	
	// Check vertex shader compilation
	GLint success;
	GLchar info_log[512];
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, 512, nullptr, info_log);
		std::cerr << "[ContextualMenuTextureTest] Vertex shader compilation failed: " << info_log << std::endl;
	}
	
	// Compile fragment shader
	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
	glCompileShader(fragment_shader);
	
	// Check fragment shader compilation
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment_shader, 512, nullptr, info_log);
		std::cerr << "[ContextualMenuTextureTest] Fragment shader compilation failed: " << info_log << std::endl;
	}
	
	// Link shaders into program
	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertex_shader);
	glAttachShader(shader_program_, fragment_shader);
	glLinkProgram(shader_program_);
	
	// Check linking
	glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(shader_program_, 512, nullptr, info_log);
		std::cerr << "[ContextualMenuTextureTest] Shader program linking failed: " << info_log << std::endl;
	}
	
	// Cleanup shaders (no longer needed after linking)
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	
	std::cout << "[ContextualMenuTextureTest] Shader program created (ID: " << shader_program_ << ")" << std::endl;
}

void ContextualMenuTextureTest::createQuad()
{

	float vertices[] = {

		-0.1f,  0.1f,   0.0f, 1.0f,  
		-0.1f, -0.1f,   0.0f, 0.0f,  
		 0.1f, -0.1f,   1.0f, 0.0f,  
		
		-0.1f,  0.1f,   0.0f, 1.0f, 
		 0.1f, -0.1f,   1.0f, 0.0f, 
		 0.1f,  0.1f,   1.0f, 1.0f 
	};
	
	// Generate and bind VAO
	glGenVertexArrays(1, &vao_);
	glBindVertexArray(vao_);
	
	// Generate and bind VBO
	glGenBuffers(1, &vbo_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	
	// Position attribute
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	
	// Texture coordinate attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	
	glBindVertexArray(0);
	
	std::cout << "[ContextualMenuTextureTest] Quad created (VAO: " << vao_ << ")" << std::endl;
}

void ContextualMenuTextureTest::render(int viewport_width, int viewport_height)
{
	if (!initialized_ || !visible_) {
		return;
	}
	
	// Enable blending for transparency
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	// Disable depth test so the overlay renders on top
	glDisable(GL_DEPTH_TEST);
	
	// Use shader program
	glUseProgram(shader_program_);
	
	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glUniform1i(glGetUniformLocation(shader_program_, "texture1"), 0);
	
	// Render quad
	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	
	// Re-enable depth test for subsequent rendering
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

void ContextualMenuTextureTest::resize(int width, int height)
{
	if (width_ == width && height_ == height) {
		return;
	}
	
	width_ = width;
	height_ = height;
	
	// Recreate texture with new size
	if (texture_id_ != 0) {
		glDeleteTextures(1, &texture_id_);
		createTexture();
	}
	
	std::cout << "[ContextualMenuTextureTest] Resized to " << width << "x" << height << std::endl;
}

void ContextualMenuTextureTest::cleanup()
{
	if (texture_id_ != 0) {
		glDeleteTextures(1, &texture_id_);
		texture_id_ = 0;
	}
	
	if (shader_program_ != 0) {
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
	}
	
	if (vao_ != 0) {
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}
	
	if (vbo_ != 0) {
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}
	
	initialized_ = false;
	
	std::cout << "[ContextualMenuTextureTest] Cleaned up" << std::endl;
}
