#pragma once

#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include <vector>
#include <iostream>
#include <list>
#include <sstream>

class ThreeDObjectSelector
{
public:
	ThreeDObjectSelector();
	
	// Print raycast debug header
	void printRaycastDebugHeader(int mouseX, int mouseY, int screenWidth, int screenHeight, 
	const glm::vec3& cameraPos, const std::vector<ThreeDObject*>& objects);

// -------- Mesh Picking --------

	void pickUpMesh(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects);
	void clearTarget();
	void select(ThreeDObject *object);

	ThreeDObject *getSelectedObject() const { return selectedObject; }

	void clearMultipleSelection();


// -------- Vertice picking --------

	Vertice* pickUpVertice(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, 
	const std::vector<ThreeDObject *> &objects, bool clearPrevious = true);

// ------- Face picking ------

	Face* pickupFace(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection,
	const std::vector<ThreeDObject *> &objects, bool clearPrevious = true);

// ------- Edge Picking ----

	Edge* pickupEdge(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection,
	const std::vector<ThreeDObject *> &objects, bool clearPrevious = true);

private:
	ThreeDObject *selectedObject = nullptr;
	std::list<ThreeDObject *> multipleSelectedObjects;

	// New proximity-based selection method
	float calculateDistanceToRay(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object);
	
	bool rayIntersectsMesh(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object, float* outDistance = nullptr);
	bool rayIntersectsBoundingBox(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object);
	bool rayIntersectsVertice(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object, const Vertice &vertice);
	bool rayIntersectsFace(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object, const Face &face, float* outDistance = nullptr);
	bool rayIntersectsEdge(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object, const Edge &edge);
	

};
