#include "WorldObjects/Face.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

// === SHADERS ===
const char* faceVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProj;
uniform mat4 model;
void main()
{
    gl_Position = viewProj * model * vec4(aPos, 1.0);
}
)";

const char* faceFragmentShaderSrc = R"(
#version 330 core
layout(location = 0) out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 1.0, 1.0, 1.0); 
}
)";


Face::Face(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
           Edge* e0, Edge* e1, Edge* e2, Edge* e3)
{
    vertices = {v0, v1, v2, v3};
    edges = {e0, e1, e2, e3};
}

Face::~Face()
{
    destroy();
}

void Face::compileShaders()
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &faceVertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &faceFragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Face::initialize()
{
    compileShaders();

    std::vector<float> faceData;
    for (Vertice* v : vertices)
    {
        glm::vec3 pos = v->getLocalPosition(); 
        faceData.push_back(pos.x);
        faceData.push_back(pos.y);
        faceData.push_back(pos.z);
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, faceData.size() * sizeof(float), faceData.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void Face::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
    glUseProgram(shaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}


void Face::destroy()
{
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
}


const std::vector<Vertice*>& Face::getVertices() const
{
    return vertices;
}

const std::vector<Edge*>& Face::getEdges() const
{
    return edges;
}
