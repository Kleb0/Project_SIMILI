#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <iostream>
#include <list>

class MeshBatch {
public:
    MeshBatch(const glm::vec3& origin, const glm::vec3& size);

    // void addVertex(Vertex* v);
    // void addEdge(Edge* e);
    // void addFace(Face* f);

    void rebuildMesh(); 
    void render(const glm::mat4& viewProj);

    const glm::vec3& getOrigin() const;
    const glm::vec3& getSize() const;

    // std::vector<Vertex*> getVerticesInVolume(const glm::vec3& min, const glm::vec3& max) const;
    // std::vector<Face*> getFacesByNormal(const glm::vec3& direction, float tolerance = 0.01f) const;

private:
    glm::vec3 origin;
    glm::vec3 size;

    // std::vector<Vertex*> vertices;
    // std::vector<Edge*> edges;
    // std::vector<Face*> faces;

    // GLuint vao = 0, vbo = 0;
    std::vector<float> vertexBuffer; 
};
