#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
#include <sstream>
#include <iostream>


const char* edgeVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 viewProj;
void main()
{
    gl_Position = viewProj * vec4(aPos, 1.0);
}
)";

const char* edgeFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 color;
void main()
{
    FragColor = color;
}
)";

Edge::Edge(Vertice* start, Vertice* end)
    : v1(start), v2(end)
{
    id = generateEdgeID();
}

std::string Edge::generateEdgeID()
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

Edge::~Edge()
{
    destroy();
}

void Edge::compileShaders()
{
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &edgeVertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &edgeFragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void Edge::initialize()
{
    compileShaders();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
}

void Edge::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
    if (!v1 || !v2) return;

    glm::vec3 p1 = glm::vec3(modelMatrix * glm::vec4(v1->getLocalPosition(), 1.0f));
    glm::vec3 p2 = glm::vec3(modelMatrix * glm::vec4(v2->getLocalPosition(), 1.0f));

    float vertices[] = {
        p1.x, p1.y, p1.z,
        p2.x, p2.y, p2.z
    };

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));


   
    glm::vec4 finalColor = edgeSelected
        ? glm::vec4(1.0f, 0.5f, 0.0f, 1.0f)  
        : color;                              

    glUniform4fv(glGetUniformLocation(shaderProgram, "color"), 1, glm::value_ptr(finalColor));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);

    glBindVertexArray(0);
}

void Edge::destroy()
{
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
}

Vertice* Edge::getStart() const { return v1; }
Vertice* Edge::getEnd() const { return v2; }

void Edge::setSelected(bool isSelected) { edgeSelected = isSelected; }
bool Edge::isSelected() const { return edgeSelected; }

void Edge::setColor(const glm::vec4& c) { color = c; }
glm::vec4 Edge::getColor() const { return color; }


void Edge::setSharedFaces(const std::vector<Face*>& faces)
{
    sharedFaces = faces;
    for (const auto& f : faces)
    {
        if (dynamic_cast<class Quad*>(f))
        {
            quadEdge = true;
            break;
        }
    }
}



const std::vector<Face*>& Edge::getSharedFaces() const
{
    return sharedFaces;
}

std::vector<Vertice*> Edge::insertVerticesAlongEdge(int count, Mesh* parentMesh)
{
    std::vector<Vertice*> newVertices;
    if (!v1 || !v2 || !parentMesh || count < 1) return newVertices;

    glm::vec3 p1 = v1->getLocalPosition();
    glm::vec3 p2 = v2->getLocalPosition();
    for (int i = 1; i <= count; ++i)
    {
        float t = float(i) / float(count + 1);
        glm::vec3 pos = (1.0f - t) * p1 + t * p2;
        Vertice* v = parentMesh->addVertice(pos);
        newVertices.push_back(v);
    }
    return newVertices;
}

void Edge::splitEdge(Vertice* newVertice, Mesh* parentMesh)
{
    if (!v1 || !v2 || !newVertice || !parentMesh) return;

    Edge* e1 = parentMesh->addEdge(v1, newVertice);
    Edge* e2 = parentMesh->addEdge(newVertice, v2);
    for (Face* f : parentMesh->getFaces())
    {
        if (!f) continue;
        auto& faceEdges = f->getEdgesNonConst();
        for (size_t i = 0; i < faceEdges.size(); ++i)
        {
            if (faceEdges[i] == this)
            {
                faceEdges[i] = e1;
                break;
            }
        
        }
    }

    e1->initialize();
    e2->initialize();

    auto& meshEdges = parentMesh->getEdgesNonConst();
    auto it = std::find(meshEdges.begin(), meshEdges.end(), this);
    if (it != meshEdges.end())
    {
        meshEdges.erase(it);
    }
    
    this->destroy();
}
