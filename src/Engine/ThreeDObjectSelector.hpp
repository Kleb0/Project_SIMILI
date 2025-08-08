#pragma once

#include "WorldObjects/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include <vector>
#include <iostream>
#include <list>

class ThreeDObjectSelector
{
public:
    ThreeDObjectSelector();

// -------- Mesh Picking --------

    void pickUpMesh(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects);
    void clearTarget();
    void select(ThreeDObject *object);

    ThreeDObject *getSelectedObject() const { return selectedObject; }

    void clearMultipleSelection();


// -------- Vertice picking --------

    Vertice* pickUpVertice(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, 
const std::vector<ThreeDObject *> &objects, bool clearPrevious = true);


private:
    ThreeDObject *selectedObject = nullptr;
    std::list<ThreeDObject *> multipleSelectedObjects;

    bool rayIntersectsMesh(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object);
    bool ThreeDObjectSelector::rayIntersectsVertice(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object, const Vertice &vertice);



};
