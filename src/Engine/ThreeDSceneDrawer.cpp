#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Engine/ThreeDSceneDrawer.hpp"
#include <iostream>
#include <filesystem>
#include <vector>

unsigned int gridVAO = 0, gridVBO = 0;
unsigned int shaderProgram = 0;

const char *vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProj;
void main()
{
    gl_Position = viewProj * vec4(aPos, 1.0);
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

ThreeDSceneDrawer::ThreeDSceneDrawer() {}

void compileShaders()
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

void ThreeDSceneDrawer::initialization()
{
    std::filesystem::path rootPath = std::filesystem::current_path().parent_path();
    std::string imagePath = (rootPath / "assets/images/cat.png").string();

    std::cout << "[DEBUG] Répertoire courant : " << std::filesystem::current_path() << std::endl;
    std::cout << "[DEBUG] Chemin construit : " << imagePath << std::endl;

    compileShaders();

    std::vector<float> gridVertices;
    for (int i = 0; i <= 5; ++i)
    {
        gridVertices.insert(gridVertices.end(), {0.0f, 0.0f, (float)i, 5.0f, 0.0f, (float)i});
        gridVertices.insert(gridVertices.end(), {(float)i, 0.0f, 0.0f, (float)i, 0.0f, 5.0f});
    }

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);

    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void ThreeDSceneDrawer::add(ThreeDObject &object)
{
    object.initialize();
    objects.push_back(&object);
}

void ThreeDSceneDrawer::render(const glm::mat4 &viewProj)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    drawBackgroundGradient();
    glEnable(GL_DEPTH_TEST);

    glUseProgram(shaderProgram);
    unsigned int viewProjLoc = glGetUniformLocation(shaderProgram, "viewProj");
    glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    for (auto *obj : objects)
    {
        obj->render(viewProj);
    }
}

void ThreeDSceneDrawer::drawBackgroundGradient()
{
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}