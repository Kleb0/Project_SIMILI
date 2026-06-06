#include "RaycastPerform.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "../../WorldObjects/Entities/ThreeDObject.hpp"
#include "../../Engine/ThreeDModes/ThreeDMode.hpp"
#include "../../WorldObjects/Basic/Edge.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <cstring>

RaycastPerform::RaycastPerform(ThreeDObjectSelector* selector)
    : selector_(selector)
{
}

RaycastPerform::~RaycastPerform() {}

void RaycastPerform::printRaycastDebugHeader(int mouseX, int mouseY, int viewportWidth, int viewportHeight,
    const glm::vec3& cameraPos, const std::vector<ThreeDObject*>& objects)
{
    std::cout << "\n ========== RAYCAST DEBUG ==========" << std::endl;
    std::cout << "[RaycastPerform] Mouse: (" << mouseX << ", " << mouseY << ")" << std::endl;
    std::cout << "[RaycastPerform] Viewport Resolution: " << viewportWidth << "x" << viewportHeight << std::endl;
    std::cout << "[RaycastPerform] Camera position: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;

    for (const auto* obj : objects) {
        if (!obj) continue;
        std::cout << "[RaycastPerform] Object: " << obj->getName()
                  << " | Selectable: " << (obj->isSelectable() ? "YES" : "NO")
                  << " | Pos: (" << obj->getPosition().x << ", " << obj->getPosition().y << ", " << obj->getPosition().z << ")" << std::endl;
    }

    std::cout << "[RaycastPerform] Starting raycast with " << objects.size() << " objects..." << std::endl;
}

void RaycastPerform::performRaycast(int mouseX, int mouseY, int workspaceX, int workspaceY, int workspaceW, int workspaceH, 
const glm::mat4& view, const glm::mat4& projection, const std::vector<ThreeDObject*>& objects, ThreeDMode* mode)
{
    if (!selector_)
        return;

    if (workspaceW <= 0 || workspaceH <= 0)
        return;

    int localX = mouseX - workspaceX;
    int localY = mouseY - workspaceY;

    if (localX < 0 || localY < 0 || localX > workspaceW || localY > workspaceH)
        return;


    // ----------- Normal Mode ------------- //
    bool isNormalMode = false;
    if (mode && std::strcmp(mode->getName(), "Normal Mode") == 0)
    {
        isNormalMode = true;
    }
    if(isNormalMode)
    {
        selector_->pickUpMesh(localX, localY, workspaceW, workspaceH, view, projection, objects);

        ThreeDObject* hit = selector_->getSelectedObject();

        last_hit_objects_.clear();
        if (hit)
        {
            std::cout << "[RaycastPerform] Hit: " << hit->getName() << std::endl;
            last_hit_objects_.push_back(hit);
        }
        else
        {
            std::cout << "[RaycastPerform] No hit detected." << std::endl;
        }
    }

    // ----------- Edge Mode ------------- // 

    bool isEdgeMode = false;
    if (mode && std::strcmp(mode->getName(), "Edge Mode") == 0)
    {
        isEdgeMode = true;
    }

    if (isEdgeMode)
    {
        last_hit_edges_.clear();
        Edge* hitEdge = selector_->pickupEdge(localX, localY, workspaceW, workspaceH, view, projection, objects, true);
        if (hitEdge)
        {
            std::cout << "[RaycastPerform] Edge Hit: " << hitEdge->getID() << std::endl;
            last_hit_edges_.push_back(hitEdge);
        }
        else
        {
            std::cout << "[RaycastPerform] No edge hit detected." << std::endl;
        }
    }

    // ---------------- Vertice Mode -------------- //

    bool isVerticeMode = false;
    if(mode && std::strcmp(mode->getName(), "Vertice Mode") == 0)
    {
        isVerticeMode = true;
    }

    if(isVerticeMode)
    {
        last_hit_vertices_.clear();
        Vertice* hitVert = selector_->pickUpVertice(localX, localY, workspaceW, workspaceH, view, projection, objects, true);
        if (hitVert)
        {
            std::cout << "[RaycastPerform] Vertice Hit: " << hitVert->getID() << std::endl;
            last_hit_vertices_.push_back(hitVert);
        }
        else
        {
            std::cout << "[RaycastPerform] No vertice hit detected." << std::endl;
        }
    } 

    // ---------------- Face Mode -------------- //

    bool isFaceMode = false;
    if(mode && std::strcmp(mode->getName(), "Face Mode") == 0)
    {
        isFaceMode = true;
    }   

    if(isFaceMode)
    {
        last_hit_faces_.clear();
        Face* hitFace = selector_->pickupFace(localX, localY, workspaceW, workspaceH, view, projection, objects, true);
        if (hitFace)
        {
            std::cout << "[RaycastPerform] Face Hit: " << hitFace->getID() << std::endl;
            last_hit_faces_.push_back(hitFace);
        }
        else
        {
            std::cout << "[RaycastPerform] No face hit detected." << std::endl;
        }
    }
}


const std::vector<ThreeDObject*>& RaycastPerform::getLastHitObjects() const
{
    return last_hit_objects_;
}

void RaycastPerform::setLastHitObjects(const std::vector<ThreeDObject*>& objects)
{
    last_hit_objects_ = objects;
}


void RaycastPerform::addLastHitObject(ThreeDObject* obj)
{
    if (obj != nullptr)
    {
        last_hit_objects_.push_back(obj);
    }
}

void RaycastPerform::clearLastHitObjects()
{
    last_hit_objects_.clear();
}

const std::vector<Edge*>& RaycastPerform::getLastHitEdges() const
{
    return last_hit_edges_;
}

void RaycastPerform::setLastHitEdges(const std::vector<Edge*>& edges)
{
    last_hit_edges_ = edges;
}

const std::vector<Vertice*>& RaycastPerform::getLastHitVertices() const
{
    return last_hit_vertices_;
}

void RaycastPerform::clearLastHitEdges()
{
    last_hit_edges_.clear();
}

void RaycastPerform::setLastHitVertices(const std::vector<Vertice*>& vertices)
{
    last_hit_vertices_ = vertices;
}

void RaycastPerform::addLastHitEdge(Edge* edge)
{
    if (edge != nullptr)
    {
        last_hit_edges_.push_back(edge);
    }
}

void RaycastPerform::addLastHitVertice(Vertice* vertice)
{
    if (vertice != nullptr)
    {
        last_hit_vertices_.push_back(vertice);
    }
}

void RaycastPerform::clearLastHitVertices()
{
    last_hit_vertices_.clear();
}

const std::vector<Face*>& RaycastPerform::getLastHitFaces() const
{
    return last_hit_faces_;
}

void RaycastPerform::setLastHitFaces(const std::vector<Face*>& faces)
{
    last_hit_faces_ = faces;
}

void RaycastPerform::addLastHitFaces(Face* face)
{
    if (face != nullptr)
    {
        last_hit_faces_.push_back(face);
    }
}

void RaycastPerform::clearLastHitFaces()
{
    last_hit_faces_.clear();
}