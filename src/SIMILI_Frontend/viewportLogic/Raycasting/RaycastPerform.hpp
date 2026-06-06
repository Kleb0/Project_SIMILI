#pragma once

#include <glm/glm.hpp>
#include <vector>

class ThreeDObjectSelector;
class ThreeDObject;
class ThreeDMode;
class Edge;
class Vertice;
class Face;

class RaycastPerform {
public:
    explicit RaycastPerform(ThreeDObjectSelector* selector);
    ~RaycastPerform();

    void performRaycast(
        int mouseX, int mouseY,
        int workspaceX, int workspaceY,
        int workspaceW, int workspaceH,
        const glm::mat4& view,
        const glm::mat4& projection,
        const std::vector<ThreeDObject*>& objects,
        ThreeDMode* mode = nullptr);

    void printRaycastDebugHeader(int mouseX, int mouseY, int viewportWidth, int viewportHeight,
        const glm::vec3& cameraPos, const std::vector<ThreeDObject*>& objects);

    const std::vector<ThreeDObject*>& getLastHitObjects() const;
    void setLastHitObjects(const std::vector<ThreeDObject*> &objects);
    void addLastHitObject(ThreeDObject* obj);
    void clearLastHitObjects();

    const std::vector<Edge*>& getLastHitEdges() const;
    void setLastHitEdges(const std::vector<Edge*> &edges);
    void addLastHitEdge(Edge* edge);
    void clearLastHitEdges();

    const std::vector<Vertice*>& getLastHitVertices() const;
    void setLastHitVertices(const std::vector<Vertice*> &vertices);
    void addLastHitVertice(Vertice* vertice);
    void clearLastHitVertices();

    const std::vector<Face*>& getLastHitFaces() const;
    void setLastHitFaces(const std::vector<Face*> &faces);
    void addLastHitFaces(Face* face);
    void clearLastHitFaces();

    std::vector<ThreeDObject*> last_hit_objects_;
    std::vector<Edge*> last_hit_edges_;
    std::vector<Vertice*> last_hit_vertices_;
    std::vector<Face*> last_hit_faces_;


private:
    ThreeDObjectSelector* selector_;
};
