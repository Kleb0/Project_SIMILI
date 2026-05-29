#include "WorldObjects/Basic/Vertice.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <sstream>
#include <random>

const char* kVerticeVertexShaderGLSL = R"(
#version 450
layout(location = 0) in vec3 aPosition;
layout(push_constant) uniform PushData {
    mat4 mvp;
} push;
void main() {
    gl_PointSize = 6.0;
    gl_Position = push.mvp * vec4(aPosition, 1.0);
}
)";

const char* kVerticeFragmentShaderGLSL = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(0.0, 1.0, 0.0, 1.0); // vert
}
)";

Vertice::Vertice() {
    id = generateVerticeID();
}

std::string Vertice::generateVerticeID()
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static const size_t idLength = 12;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    std::stringstream ss;
    for (size_t i = 0; i < idLength; ++i)
        ss << charset[dis(gen)];
    return ss.str();
}

Vertice::~Vertice()
{
    destroy();
}


void Vertice::initialize()
{
    // TEMPORAIRE: Désactivation OpenGL pour éviter crash avec moteur Vulkan
    // TODO: Implémenter rendu Vulkan pour les vertices
    std::cout << "[Vertice] Initialize called for vertex ID: " << id << std::endl;
    
    // compileShaders();  // <- Commenté temporairement (appels OpenGL)
    
    // float vertex[] = { 0.0f, 0.0f, 0.0f };
    
    // glGenVertexArrays(1, &vao);      // <- Commenté (OpenGL)
    // glGenBuffers(1, &vbo);           // <- Commenté (OpenGL)
    
    // glBindVertexArray(vao);
    // glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);
    
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    // glEnableVertexAttribArray(0);
    
    // glBindVertexArray(0);
    
    std::cout << "[Vertice] Vertex initialized (OpenGL disabled)" << std::endl;
}

void Vertice::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
}

void Vertice::destroy()
{
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

void Vertice::setMeshParent(ThreeDObject* parent)
{
    meshParent = parent;
}

ThreeDObject* Vertice::getMeshParent() const
{
    return meshParent;
}

void Vertice::setSelected(bool isSelected)
{
    VerticeSelected = isSelected;
}

bool Vertice::isSelected() const
{
    return VerticeSelected;
}

void Vertice::applyTranslationToLocal(const glm::vec3& translation, const glm::mat4& parentModelMatrix)
{
    glm::mat4 invParent = glm::inverse(parentModelMatrix);
    glm::vec3 localTranslation = glm::vec3(invParent * glm::vec4(translation, 0.0f));
    localPosition += localTranslation;
}

void Vertice::addEdge(Edge* e)
{
    if (e && std::find(edges.begin(), edges.end(), e) == edges.end())
        edges.push_back(e);
}

const std::vector<Edge*>& Vertice::getEdges() const
{
    return edges;
}

void Vertice::removeEdge(Edge* e)
{
    auto it = std::find(edges.begin(), edges.end(), e);
    if (it != edges.end())
        edges.erase(it);
}