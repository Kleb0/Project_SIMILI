#include "TextureRendererTest.hpp"
#include <iostream>
#include <vector>
#include <cmath>

TextureRendererTest::TextureRendererTest()
	: texture_id_(0)
	, vao_(0)
	, vbo_(0)
	, shader_program_(0)
	, width_(0)
	, height_(0)
	, texture_width_(250)
	, texture_height_(100)
	, initialized_(false)
{
}

TextureRendererTest::~TextureRendererTest()
{
	cleanup();
}

void TextureRendererTest::initialize(int width, int height)
{
	if (initialized_) 
	{
		return;
	}

	width_ = width;
	height_ = height;

	createTexture(texture_width_, texture_height_);
	createQuadMesh();
	createShaderProgram();

	initialized_ = true;
}

void TextureRendererTest::createTexture(int tex_width, int tex_height)
{
	texture_width_ = tex_width;
	texture_height_ = tex_height;
	
	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width_, texture_height_, 0, 
				 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

}

void TextureRendererTest::createQuadMesh()
{

	float vertices[] = 
	{

		0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 
		0.0f, 0.0f, 0.0f,   0.0f, 0.0f,  
		1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
		 
		0.0f, 1.0f, 0.0f,   0.0f, 1.0f,  // Top-left
		1.0f, 0.0f, 0.0f,   1.0f, 0.0f,  // Bottom-right
		1.0f, 1.0f, 0.0f,   1.0f, 1.0f   // Top-right
	};

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);

	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// TexCoord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

}

GLuint TextureRendererTest::compileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	// Check for compilation errors
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) 
	{
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, nullptr, infoLog);
		std::cerr << "[TextureRendererTest] Shader compilation failed:\n" << infoLog << std::endl;
	}

	return shader;
}

void TextureRendererTest::createShaderProgram()
{
	const char* vertexShaderSource = R"(
		#version 460 core
		layout (location = 0) in vec3 aPos;
		layout (location = 1) in vec2 aTexCoord;
		
		out vec2 TexCoord;
		
		uniform vec2 viewportSize;
		uniform vec2 rectSize;
		uniform vec2 rectPos;
		
		void main()
		{
			// Transform from [0,1] to pixel coordinates
			vec2 pixelPos = aPos.xy * rectSize + rectPos;
			
			// Convert pixel coordinates to normalized device coordinates [-1, 1]
			vec2 ndc = (pixelPos / viewportSize) * 2.0 - 1.0;
			ndc.y = -ndc.y; // Flip Y axis (OpenGL convention)
			
			gl_Position = vec4(ndc, 0.0, 1.0);
			TexCoord = aTexCoord;
		}
	)";

	const char* fragmentShaderSource = R"(
		#version 460 core
		out vec4 FragColor;
		
		in vec2 TexCoord;
		uniform sampler2D texture1;
		
		void main()
		{
			FragColor = texture(texture1, TexCoord);
		}
	)";

	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertexShader);
	glAttachShader(shader_program_, fragmentShader);
	glLinkProgram(shader_program_);

	// Check for linking errors
	GLint success;
	glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(shader_program_, 512, nullptr, infoLog);
		std::cerr << "[TextureRendererTest] Shader linking failed:\n" << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void TextureRendererTest::render()
{
	if (!initialized_) {
		return;
	}

	// Save current OpenGL state
	GLboolean depthTestEnabled;
	glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
	GLboolean blendEnabled;
	glGetBooleanv(GL_BLEND, &blendEnabled);

	// Setup for overlay rendering
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Use shader program
	glUseProgram(shader_program_);

	// Set uniforms for positioning
	glUniform2f(glGetUniformLocation(shader_program_, "viewportSize"), 
				static_cast<float>(width_), static_cast<float>(height_));
	glUniform2f(glGetUniformLocation(shader_program_, "rectSize"), 
				static_cast<float>(render_width_), static_cast<float>(render_height_));
	glUniform2f(glGetUniformLocation(shader_program_, "rectPos"), 
				static_cast<float>(render_x_), static_cast<float>(render_y_));

	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glUniform1i(glGetUniformLocation(shader_program_, "texture1"), 0);

	// Render quad
	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	// Restore OpenGL state
	if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
	if (!blendEnabled) glDisable(GL_BLEND);
}

void TextureRendererTest::updateTexture(const void* buffer, int width, int height)
{
	if (!initialized_ || !buffer) {
		return;
	}
	
	// If size changed, recreate texture
	if (width != texture_width_ || height != texture_height_) {
		if (texture_id_ != 0) {
			glDeleteTextures(1, &texture_id_);
		}
		createTexture(width, height);
	}
	
	// Update texture data (CEF uses BGRA format)
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture_width_, texture_height_,
					GL_BGRA, GL_UNSIGNED_BYTE, buffer);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void TextureRendererTest::resize(int width, int height)
{
	if (width == width_ && height == height_) {
		return;
	}


	width_ = width;
	height_ = height;
	
	static int lastLoggedWidth = 0;
	static int lastLoggedHeight = 0;
	if (abs(width - lastLoggedWidth) > 50 || abs(height - lastLoggedHeight) > 50) 
	{
		lastLoggedWidth = width;
		lastLoggedHeight = height;
	}
}

void TextureRendererTest::setRenderRect(int x, int y, int w, int h)
{
	render_x_ = x;
	render_y_ = y;
	render_width_ = w;
	render_height_ = h;
	
}

void TextureRendererTest::cleanup()
{
	if (!initialized_) {
		return;
	}

	if (texture_id_ != 0) {
		glDeleteTextures(1, &texture_id_);
		texture_id_ = 0;
	}

	if (vao_ != 0) {
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}

	if (vbo_ != 0) {
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}

	if (shader_program_ != 0) {
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
	}

	initialized_ = false;
}
