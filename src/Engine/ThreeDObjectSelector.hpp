#pragma once

#include "WorldObjects/ThreeDObject.hpp"
#include <vector>
#include <iostream>
#include <list>

class ThreeDObjectSelector
{
public:
    ThreeDObjectSelector();
    void pickUpTarget(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects);
    // void update(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects);
    void clearTarget();
    void select(ThreeDObject *object);

    ThreeDObject *getSelectedObject() const { return selectedObject; }

    // multiple selection
    void pickupMultipleTargets(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects);
    void clearMultipleSelection();

private:
    ThreeDObject *selectedObject = nullptr;
    std::list<ThreeDObject *> multipleSelectedObjects;

    bool rayIntersectsCube(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object);
};
