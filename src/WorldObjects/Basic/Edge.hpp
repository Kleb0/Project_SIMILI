

#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>

// Forward declarations
class Vertice;
class Mesh;

class Edge
{
public:
    Edge(Vertice* start, Vertice* end);
    ~Edge();

    void initialize();
    void render(const glm::mat4& viewProj, const glm::mat4& modelMatrix);
    void destroy();

    std::vector<Vertice*> insertVerticesAlongEdge(int count, Mesh* parentMesh);

    Vertice* getStart() const;
    Vertice* getEnd() const;

    void setSelected(bool isSelected);
    bool isSelected() const;

    void setColor(const glm::vec4& c);
    glm::vec4 getColor() const;

    void splitEdge(Vertice* newVertice, Mesh* parentMesh);

    std::string getID() const { return id; }
    void setSharedFaces(const std::vector<class Face*>& faces);
    const std::vector<class Face*>& getSharedFaces() const;
    std::vector<class Face*>& getSharedFacesNonConst() { return sharedFaces; }

private:
    Vertice* v1;
    Vertice* v2;

    std::vector<class Face*> sharedFaces;
    bool quadEdge = false;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); 
    bool edgeSelected = false;

    void compileShaders();

    std::string id;
    static std::string generateEdgeID();
};