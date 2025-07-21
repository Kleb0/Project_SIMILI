#include "WorldObjects/Cube.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

const char *cubeVertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 viewProj;
void main()
{
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

const char *cubeFragmentShaderSource = R"(
#version 330 core
layout(location = 0) out vec4 FragColor;
uniform vec4 color;
void main()
{
    FragColor = color;
}
)";

Cube::Cube() {}
Cube::~Cube()
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(shaderProgram);
}

void Cube::compileShaders()
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &cubeVertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &cubeFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Cube::createVertices()
{
    glm::vec3 localPositions[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    glm::mat4 modelMatrix = getModelMatrix();

    for (int i = 0; i < 8; ++i)
    {
        Vertice* vert = new Vertice();
        glm::vec4 worldPos = modelMatrix * glm::vec4(localPositions[i], 1.0f);
        vert->setPosition(glm::vec3(worldPos));
        vert->setLocalPosition(localPositions[i]); 
        vert->initialize();
        vert->setName("Vertice_" + std::to_string(i));
        vertices.push_back(vert);
    }
}

void Cube::initialize()
{
    compileShaders();

    createVertices();

    glm::mat4 modelMatrix = getModelMatrix();

    glm::vec3 localPositions[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    unsigned int cubeIndices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 4, 7, 7, 3, 0,
        1, 5, 6, 6, 2, 1,
        3, 2, 6, 6, 7, 3,
        0, 1, 5, 5, 4, 0
    };

    std::vector<float> finalVertexData;
    for (int i = 0; i < 36; ++i)
    {
        glm::vec4 worldPos = modelMatrix * glm::vec4(localPositions[cubeIndices[i]], 1.0f);
        finalVertexData.push_back(worldPos.x);
        finalVertexData.push_back(worldPos.y);
        finalVertexData.push_back(worldPos.z);
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, finalVertexData.size() * sizeof(float), finalVertexData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}


void Cube::render(const glm::mat4& viewProj)
{
    glm::mat4 modelMatrix = getModelMatrix();

    // --- Render Cube
    glUseProgram(shaderProgram);

    glUniform4f(glGetUniformLocation(shaderProgram, "color"), 0.3f, 0.7f, 0.9f, 0.3f);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);

    glm::vec3 localPositions[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    for (int i = 0; i < vertices.size(); ++i)
    {

        vertices[i]->render(viewProj, modelMatrix);
    }
}

const std::vector<Vertice*>& Cube::getVertices() const
{
    return vertices;
}


void Cube::destroy()
{

    for (Vertice* vert : vertices)
    {
        if (vert)
        {
            vert->destroy();
            delete vert;
        }
    }
    vertices.clear();

    if (vao != 0)
    {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }

    if (vbo != 0)
    {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }

    if (shaderProgram != 0)
    {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }

    

    std::cout << "[Cube] Resources destroyed manually." << std::endl;
}
