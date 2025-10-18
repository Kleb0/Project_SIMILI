#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

class Vertice;
class Edge;

class Face
{
public:

    Face(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
        Edge* e0, Edge* e1, Edge* e2, Edge* e3);
    virtual ~Face();

    void initialize();
    void render(const glm::mat4& viewProj, const glm::mat4& modelMatrix);
    void destroy();

    const std::vector<Vertice*>& getVertices() const;
    const std::vector<Edge*>& getEdges() const;
    std::vector<Edge*>& getEdgesNonConst() { return edges; }

    const glm::mat4& getFaceTransform() const { return faceTransform; }
    void setFaceTransform(const glm::mat4& m) { faceTransform = m; }
    void applyWorldDelta(const glm::mat4& deltaWorld, const glm::mat4& parentModel, bool bakeToVertices);

    void setSelected(bool v) { selected = v; }
    bool isSelected() const { return selected; }

    void setParentMesh(class Mesh* mesh);
    class Mesh* getParentMesh() const;

    void setColor(const glm::vec4& c);
    const glm::vec4& getColor() const;

    std::string getID() const { return id; }
    bool isJoiningQuad = false;

protected:
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    bool selected = false;
    glm::mat4 faceTransform = glm::mat4(1.0f);

    Mesh* parentMesh = nullptr;

    glm::vec4 color = glm::vec4(1.0f); 

    std::string id;
    static std::string generateFaceID();

    void compileShaders();
    void uploadFromVertices();
};
