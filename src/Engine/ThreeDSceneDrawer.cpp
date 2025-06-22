#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Engine/ThreeDSceneDrawer.hpp"
#include <iostream>
#include <filesystem>
#include <vector>

// === SHADERS ===

const char *vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProj;
uniform mat4 model;
void main()
{
    
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)";

// === CLASS ===
ThreeDSceneDrawer::ThreeDSceneDrawer() {}

void ThreeDSceneDrawer::compileShaders()
{
	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
	glCompileShader(vertexShader);

	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void ThreeDSceneDrawer::initizalize()
{
	compileShaders();

	std::vector<float> gridVertices;
	int gridSize = 10;
	float step = 1.0f;

	for (int i = -gridSize / 2; i <= gridSize / 2; ++i)
	{
		gridVertices.insert(gridVertices.end(), {
			-gridSize / 2.0f, 0.0f, (float)i,
			gridSize / 2.0f, 0.0f, (float)i
		});

		gridVertices.insert(gridVertices.end(), {
			(float)i, 0.0f, -gridSize / 2.0f,
			(float)i, 0.0f,  gridSize / 2.0f
		});
	}

	glGenVertexArrays(1, &gridVAO);
	glGenBuffers(1, &gridVBO);
	glBindVertexArray(gridVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
	glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glGenTextures(1, &fboTexture);
	glBindTexture(GL_TEXTURE_2D, fboTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexture, 0);

	glGenRenderbuffers(1, &rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Error : Uncomplete framebuffer !" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void ThreeDSceneDrawer::resize(int w, int h)
{
	width = w;
	height = h;

	glBindTexture(GL_TEXTURE_2D, fboTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glBindRenderbuffer(GL_RENDERBUFFER, rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
}

void ThreeDSceneDrawer::render(const std::list<ThreeDObject *> &objects, const glm::mat4 &viewProj)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	drawBackgroundGradient();
	glEnable(GL_DEPTH_TEST);

	glUseProgram(shaderProgram);
	unsigned int viewProjLoc = glGetUniformLocation(shaderProgram, "viewProj");
	glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

	glBindVertexArray(gridVAO);
	glm::mat4 model = glm::mat4(1.0f);
	unsigned int modeLoc = glGetUniformLocation(shaderProgram, "model");
	glUniformMatrix4fv(modeLoc, 1, GL_FALSE, glm::value_ptr(model));
	glDrawArrays(GL_LINES, 0, 44);
	glBindVertexArray(0);

	for (auto *obj : objects)
	{
		if (obj)
		{
			// std::cout << " Rendering object with name : - " << obj->getName() << std::endl;
			obj->render(viewProj);
		}
	}
}

void ThreeDSceneDrawer::add(ThreeDObject *object)
{
	if (object)
		objects.push_back(object);
}

void ThreeDSceneDrawer::drawBackgroundGradient()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}