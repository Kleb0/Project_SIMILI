#include "Engine/ThreeDObjectSelector.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <limits>
#include <iostream>

ThreeDObjectSelector::ThreeDObjectSelector()
{
}

void ThreeDObjectSelector::pickUpMesh(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects)
{
    float x = (2.0f * mouseX) / screenWidth - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / screenHeight;

    glm::vec3 rayStart = glm::unProject(glm::vec3(mouseX, mouseY, 0.0f), view, projection, glm::vec4(0, 0, screenWidth, screenHeight));
    glm::vec3 rayEnd = glm::unProject(glm::vec3(mouseX, mouseY, 1.0f), view, projection, glm::vec4(0, 0, screenWidth, screenHeight));

    glm::vec3 rayDir = glm::normalize(rayEnd - rayStart);
    glm::vec3 rayOrigin = rayStart;

    float closestDistance = std::numeric_limits<float>::max();
    ThreeDObject *closestObject = nullptr;

    for (auto *obj : objects)
    {
        if (!obj) continue;
        if (!obj->isSelectable()) continue;
        Mesh* mesh = dynamic_cast<Mesh*>(obj);
        if (mesh) {
            const auto& verts = mesh->getVertices();
            const auto& edges = mesh->getEdges();
            const auto& faces = mesh->getFaces();
            if (verts.empty() && edges.empty() && faces.empty()) continue;
        }
        if (rayIntersectsMesh(rayOrigin, rayDir, *obj))
        {
            float distance = glm::length(obj->getPosition() - rayOrigin);
            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestObject = obj;
            }
        }
    }

    selectedObject = closestObject;
}

bool ThreeDObjectSelector::rayIntersectsMesh(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object)
{

    glm::mat4 model = object.getModelMatrix();
    glm::vec3 center = glm::vec3(model * glm::vec4(0, 0, 0, 1));
    glm::vec3 halfScale = object.getScale() * 0.5f;

    glm::vec3 xAxis = glm::normalize(glm::vec3(model[0]));
    glm::vec3 yAxis = glm::normalize(glm::vec3(model[1]));
    glm::vec3 zAxis = glm::normalize(glm::vec3(model[2]));

    glm::vec3 delta = center - rayOrigin;

    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();

    auto testAxis = [&](const glm::vec3 &axis, float halfSize) -> bool
    {
        float e = glm::dot(axis, delta);
        float f = glm::dot(rayDir, axis);

        if (std::abs(f) > 0.0001f)
        {
            float t1 = (e + halfSize) / f;
            float t2 = (e - halfSize) / f;

            if (t1 > t2)
                std::swap(t1, t2);
            if (t1 > tMin)
                tMin = t1;
            if (t2 < tMax)
                tMax = t2;
            if (tMin > tMax)
                return false;
        }
        else if (-e - halfSize > 0.0f || -e + halfSize < 0.0f)
        {
            return false;
        }

        return true;
    };

    return testAxis(xAxis, halfScale.x) &&
           testAxis(yAxis, halfScale.y) &&
           testAxis(zAxis, halfScale.z);
}

void ThreeDObjectSelector::clearTarget()
{
    selectedObject = nullptr;
}

void ThreeDObjectSelector::select(ThreeDObject *object)
{
    selectedObject = object;
}


void ThreeDObjectSelector::clearMultipleSelection()
{
    multipleSelectedObjects.clear();
}


// -------- Vertice Selection --------

Vertice* ThreeDObjectSelector::pickUpVertice(int mouseX, int mouseY, int screenWidth, int screenHeight, 
const glm::mat4& view, const glm::mat4& projection, const std::vector<ThreeDObject*>& objects, bool clearPrevious)
{

    glm::vec3 rayStart = glm::unProject(glm::vec3(mouseX, mouseY, 0.0f),
    view, projection, glm::vec4(0, 0, screenWidth, screenHeight));

    glm::vec3 rayEnd   = glm::unProject(glm::vec3(mouseX, mouseY, 1.0f),
    view, projection, glm::vec4(0, 0, screenWidth, screenHeight));

    glm::vec3 rayDir    = glm::normalize(rayEnd - rayStart);
    glm::vec3 rayOrigin = rayStart;

    float closestDistance = std::numeric_limits<float>::max();
    Vertice* closestVertice = nullptr;

    for (ThreeDObject* obj : objects) 
    {
        if (!obj->isSelectable()) continue;

        Mesh * mesh = dynamic_cast<Mesh*>(obj);
        if (!mesh) continue;

        if (clearPrevious) 
        {
            for (Vertice* v : mesh->getVertices())
                v->setSelected(false);
        }

        for (Vertice* v : mesh->getVertices()) 
        {
            glm::mat4 modelMatrix = obj->getModelMatrix();
            glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(v->getLocalPosition(), 1.0f));
            v->setPosition(worldPos);

            if (rayIntersectsVertice(rayOrigin, rayDir, *obj, *v)) {
                float distance = glm::length(v->getPosition() - rayOrigin);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestVertice  = v;
                }
            }
        }
    }


    return closestVertice;
}

bool ThreeDObjectSelector::rayIntersectsVertice(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object, const Vertice &vertice)
{
    const float radius = 0.15f;

    glm::vec3 vertWorldPos = vertice.getPosition();
    glm::vec3 toVert = vertWorldPos - rayOrigin;

    float projectionLength = glm::dot(toVert, rayDir);
    if (projectionLength < 0.0f)
        return false;

    glm::vec3 projectedPoint = rayOrigin + rayDir * projectionLength;
    float distance = glm::length(projectedPoint - vertWorldPos);

    return distance < radius;
}


// ---------- Face Selection ----------

Face* ThreeDObjectSelector::pickupFace(int mouseX, int mouseY, int screenWidth, int screenHeight,
const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects, bool clearPrevious)
{
    glm::vec3 rayStart = glm::unProject(glm::vec3(mouseX, mouseY, 0.0f),
    view, projection, glm::vec4(0, 0, screenWidth, screenHeight));

    glm::vec3 rayEnd   = glm::unProject(glm::vec3(mouseX, mouseY, 1.0f),
    view, projection, glm::vec4(0, 0, screenWidth, screenHeight));

    glm::vec3 rayDir    = glm::normalize(rayEnd - rayStart);
    glm::vec3 rayOrigin = rayStart;

    float closestDistance = std::numeric_limits<float>::max();
    Face* closestFace = nullptr;

    for (ThreeDObject* obj : objects)
    {
        if (!obj || !obj->isSelectable()) continue;

        Mesh* mesh = dynamic_cast<Mesh*>(obj);
        if (!mesh) continue;

        if (clearPrevious)
        {
            for (Face* f : mesh->getFaces())
                if (f) f->setSelected(false);
        }

        const glm::mat4 model = obj->getModelMatrix();

        for (Face* f : mesh->getFaces())
        {
            if (!f) continue;

            if (!rayIntersectsFace(rayOrigin, rayDir, *obj, *f))
                continue;

            const auto& verts = f->getVertices();
            if (verts.size() < 4) continue;

            glm::vec3 p0 = glm::vec3(model * glm::vec4(verts[0]->getLocalPosition(), 1.0f));
            glm::vec3 p1 = glm::vec3(model * glm::vec4(verts[1]->getLocalPosition(), 1.0f));
            glm::vec3 p2 = glm::vec3(model * glm::vec4(verts[2]->getLocalPosition(), 1.0f));

            glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
            float denom = glm::dot(n, rayDir);
            if (std::abs(denom) < 1e-6f) continue;

            float t = glm::dot(n, (p0 - rayOrigin)) / denom;
            if (t < 0.0f) continue;

            glm::vec3 hitPoint = rayOrigin + t * rayDir;
            float distance = glm::length(hitPoint - rayOrigin);

            if (distance < closestDistance)
            {
                closestDistance = distance;
                closestFace = f;
            }
        }
    }

    if (closestFace)
        closestFace->setSelected(true);

    return closestFace;
}

bool ThreeDObjectSelector::rayIntersectsFace(const glm::vec3 &rayOrigin, 
const glm::vec3 &rayDir, const ThreeDObject &object, const Face &face)
{
    const glm::mat4 model = object.getModelMatrix();

    const auto& verts = face.getVertices();
    if (verts.size() < 4) return false;

    glm::vec3 p0 = glm::vec3(model * glm::vec4(verts[0]->getLocalPosition(), 1.0f));
    glm::vec3 p1 = glm::vec3(model * glm::vec4(verts[1]->getLocalPosition(), 1.0f));
    glm::vec3 p2 = glm::vec3(model * glm::vec4(verts[2]->getLocalPosition(), 1.0f));
    glm::vec3 p3 = glm::vec3(model * glm::vec4(verts[3]->getLocalPosition(), 1.0f));

    glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
    float denom = glm::dot(n, rayDir);
    if (std::abs(denom) < 1e-6f)
        return false; 

    float t = glm::dot(n, (p0 - rayOrigin)) / denom;
    if (t < 0.0f)
        return false;

    glm::vec3 P = rayOrigin + t * rayDir;

    auto pointInTri = [&](const glm::vec3& A, const glm::vec3& B, const glm::vec3& C, const glm::vec3& Pnt) -> bool
    {
        glm::vec3 c0 = glm::cross(B - A, Pnt - A);
        glm::vec3 c1 = glm::cross(C - B, Pnt - B);
        glm::vec3 c2 = glm::cross(A - C, Pnt - C);
        float d0 = glm::dot(c0, n);
        float d1 = glm::dot(c1, n);
        float d2 = glm::dot(c2, n);
        const float eps = -1e-6f;
        return (d0 >= eps) && (d1 >= eps) && (d2 >= eps);
    };


    bool inside = pointInTri(p0, p1, p2, P) || pointInTri(p0, p2, p3, P);
    return inside;
}


// ----------- Edge Selection --------

bool ThreeDObjectSelector::rayIntersectsEdge(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, 
const ThreeDObject &object, const Edge &edge)
{
    const float radius = 0.15f;

    glm::mat4 model = object.getModelMatrix();
    glm::vec3 a0 = glm::vec3(model * glm::vec4(edge.getStart()->getLocalPosition(), 1.0f));
    glm::vec3 b0 = glm::vec3(model * glm::vec4(edge.getEnd()->getLocalPosition(),   1.0f));
    glm::vec3 v = b0 - a0;
    glm::vec3 u = glm::normalize(rayDir);
    glm::vec3 w0 = rayOrigin - a0;

    float a = glm::dot(u, u);
    float b = glm::dot(u, v);
    float c = glm::dot(v, v);
    float d = glm::dot(u, w0);
    float e = glm::dot(v, w0);
    float D = a * c - b * b;

    float s, t;
    if (D > 1e-6f)
    {
        s = (b * e - c * d) / D;
        t = (a * e - b * d) / D;
    }
    else
    {
        s = 0.0f;
        t = c > 0.0f ? e / c : 0.0f;
    }

    if (s < 0.0f)
    {
        s = 0.0f;
        t = c > 0.0f ? e / c : 0.0f;
    }

    t = glm::clamp(t, 0.0f, 1.0f);

    glm::vec3 pc = rayOrigin + s * u;
    glm::vec3 qc = a0 + t * v;

    float dist = glm::length(pc - qc);
    return dist < radius && s >= 0.0f;
}

Edge* ThreeDObjectSelector::pickupEdge(int mouseX, int mouseY, int screenWidth, int screenHeight, 
const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects, bool clearPrevious)
{
    glm::vec3 rayStart = glm::unProject(glm::vec3(mouseX, mouseY, 0.0f), view, projection, glm::vec4(0, 0, screenWidth, screenHeight));
    glm::vec3 rayEnd   = glm::unProject(glm::vec3(mouseX, mouseY, 1.0f), view, projection, glm::vec4(0, 0, screenWidth, screenHeight));
    glm::vec3 rayDir   = glm::normalize(rayEnd - rayStart);
    glm::vec3 rayOrigin = rayStart;

    float closestS = std::numeric_limits<float>::max();
    Edge* closestEdge = nullptr;

    for (ThreeDObject* obj : objects)
    {
        if (!obj->isSelectable()) continue;

        Mesh* mesh = dynamic_cast<Mesh*>(obj);
        if (!mesh) continue;

        if (clearPrevious)
        {
            for (Edge* e : mesh->getEdges())
                e->setSelected(false);
        }

        for (Edge* e : mesh->getEdges())
        {
            if (!e || !e->getStart() || !e->getEnd()) continue;

            if (rayIntersectsEdge(rayOrigin, rayDir, *obj, *e))
            {
                glm::mat4 model = obj->getModelMatrix();
                glm::vec3 a0 = glm::vec3(model * glm::vec4(e->getStart()->getLocalPosition(), 1.0f));
                glm::vec3 b0 = glm::vec3(model * glm::vec4(e->getEnd()->getLocalPosition(),   1.0f));
                glm::vec3 v = b0 - a0;
                glm::vec3 u = glm::normalize(rayDir);
                glm::vec3 w0 = rayOrigin - a0;

                float a = glm::dot(u, u);
                float b = glm::dot(u, v);
                float c = glm::dot(v, v);
                float d = glm::dot(u, w0);
                float ee = glm::dot(v, w0);
                float D = a * c - b * b;

                float s, t;
                if (D > 1e-6f)
                {
                    s = (b * ee - c * d) / D;
                    t = (a * ee - b * d) / D;
                }
                else
                {
                    s = 0.0f;
                    t = c > 0.0f ? ee / c : 0.0f;
                }

                if (s < 0.0f)
                {
                    s = 0.0f;
                    t = c > 0.0f ? ee / c : 0.0f;
                }

                t = glm::clamp(t, 0.0f, 1.0f);

                if (s >= 0.0f && s < closestS)
                {
                    closestS = s;
                    closestEdge = e;
                }
            }
        }
    }

    return closestEdge;
}
