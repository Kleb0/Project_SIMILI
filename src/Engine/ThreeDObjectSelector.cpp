#include "Engine/ThreeDObjectSelector.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <limits>
#include <iostream>

ThreeDObjectSelector::ThreeDObjectSelector()
{
}

void ThreeDObjectSelector::pickUpTarget(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects)
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
        if (!obj->isSelectable())
            continue;

        if (rayIntersectsCube(rayOrigin, rayDir, *obj))
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

bool ThreeDObjectSelector::rayIntersectsCube(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object)
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