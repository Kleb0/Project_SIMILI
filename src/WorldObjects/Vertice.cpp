#include "WorldObjects/Vertice.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

const char* verticeVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 viewProj;
void main()
{
    gl_PointSize = 10.0;
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

const char* verticeFragmentShader = R"(
#version 330 core
out vec4 FragColor;

uniform vec4 color;

void main()
{
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = dot(coord, coord);

    // draw a circle 
    if (dist > 0.25) 
        discard;

    FragColor = color;
}
)";

Vertice::Vertice() {}

Vertice::~Vertice()
{
    destroy();
}

void Vertice::compileShaders()
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &verticeVertexShader, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &verticeFragmentShader, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Vertice::initialize()
{
    compileShaders();

    float vertex[] = { 0.0f, 0.0f, 0.0f };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Vertice::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
    glUseProgram(shaderProgram);

    glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(localPosition, 1.0f));
    glm::mat4 renderMatrix = glm::translate(glm::mat4(1.0f), worldPos);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(renderMatrix));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));

        glm::vec4 finalColor = isSelected()
        ? glm::vec4(1.0f, 0.5f, 0.0f, 1.0f) 
        : color;                           

    glUniform4fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(finalColor));

    glEnable(GL_PROGRAM_POINT_SIZE);
    glBindVertexArray(vao);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
}

void Vertice::destroy()
{
    if (vao != 0) {
        glDeleteVertexArrays(1, &vao);
        vao = 0;
    }

    if (vbo != 0) {
        glDeleteBuffers(1, &vbo);
        vbo = 0;
    }

    if (shaderProgram != 0) {
        glDeleteProgram(shaderProgram);
        shaderProgram = 0;
    }
}

void Vertice::setColor(const glm::vec4& newColor)
{
    color = newColor;
}

glm::vec4 Vertice::getColor() const
{
    return color;
}

void Vertice::setLocalPosition(const glm::vec3& pos)
{
    localPosition = pos;
}

glm::vec3 Vertice::getLocalPosition() const
{
    return localPosition;
}

void Vertice::setPosition(const glm::vec3& pos)
{
    position = pos;
}

glm::vec3 Vertice::getPosition() const {

    return glm::vec3(getModelMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

void Vertice::setName(const std::string& newName)
{
    name = newName;
}

const std::string& Vertice::getName() const
{
    return name;
}

glm::mat4 Vertice::getModelMatrix() const
{
    return glm::translate(glm::mat4(1.0f), position);
}

void Vertice::setSelected(bool isSelected)
{
    VerticeSelected = isSelected;
}

bool Vertice::isSelected() const
{
    return VerticeSelected;
}

