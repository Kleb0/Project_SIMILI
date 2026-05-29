#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <iostream>
#include <random>
#include <sstream>

const char* kFaceVertexShaderGLSL = R"(
#version 450
layout(location = 0) in vec3 aPosition;
layout(push_constant) uniform PushData {
    mat4 mvp;
} push;
void main() {
    gl_Position = push.mvp * vec4(aPosition, 1.0);
}
)";

const char* kFaceFragmentShaderGLSL = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0); // blanc
}
)";


Face::Face(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
           Edge* e0, Edge* e1, Edge* e2, Edge* e3)
{
    vertices = {v0, v1, v2, v3};
    edges = {e0, e1, e2, e3};
    parentMesh = nullptr;
    id = generateFaceID();
}

std::string Face::generateFaceID()
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

Face::~Face()
{
    destroy();
}


void Face::initialize()
{
    // TEMPORAIRE: Désactivation OpenGL pour éviter crash avec moteur Vulkan
    std::cout << "[Face] Initialize called for face ID: " << id << std::endl;
    
    // compileShaders();           // <- Commenté temporairement (appels OpenGL)
    
    // glGenVertexArrays(1, &vao); // <- Commenté (OpenGL)
    // glGenBuffers(1, &vbo);      // <- Commenté (OpenGL)
    
    // glBindVertexArray(vao);
    // glBindBuffer(GL_ARRAY_BUFFER, vbo);
    // glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    // glEnableVertexAttribArray(0);
    // glBindVertexArray(0);
    
    std::cout << "[Face] Face initialized (OpenGL disabled)" << std::endl;
}


void Face::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
}



void Face::destroy()
{
}


const std::vector<Vertice*>& Face::getVertices() const
{
    return vertices;
}

const std::vector<Edge*>& Face::getEdges() const
{
    return edges;
}

void Face::setParentMesh(Mesh* mesh)
{
    parentMesh = mesh;
}

Mesh* Face::getParentMesh() const
{
    return parentMesh;
}

void Face::applyWorldDelta(const glm::mat4& deltaWorld, const glm::mat4& parentModel, bool bakeToVertices)
{
    if (!bakeToVertices)
    {
        glm::mat4 faceLocalDelta = glm::inverse(parentModel) * deltaWorld * parentModel;
        faceTransform = faceLocalDelta * faceTransform;
        return;
    }


    for (auto* v : vertices)
    {
        glm::vec4 L  = glm::vec4(v->getLocalPosition(), 1.0f);
        glm::vec4 W  = parentModel * faceTransform * L;
        glm::vec4 W2 = deltaWorld * W;
        glm::vec4 L2 = glm::inverse(parentModel) * W2;

        v->setLocalPosition(glm::vec3(L2));
        v->setPosition(glm::vec3(W2)); 
    }

    faceTransform = glm::mat4(1.0f);
}

void Face::setColor(const glm::vec4& c)
{
    color = c;
}

const glm::vec4& Face::getColor() const
{
    return color;
}