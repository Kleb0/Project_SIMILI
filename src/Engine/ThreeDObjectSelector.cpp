#include "Engine/ThreeDObjectSelector.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <limits>
#include <iostream>

#include "Engine/ErrorBox.hpp"

ThreeDObjectSelector::ThreeDObjectSelector()
{
}

void ThreeDObjectSelector::printRaycastDebugHeader(int mouseX, int mouseY, int screenWidth, int screenHeight, 
const glm::vec3& cameraPos, const std::vector<ThreeDObject*>& objects)
{
	std::cout << "\n ========== RAYCAST DEBUG ==========" << std::endl;
	std::cout << "[OverlayViewport] Mouse: (" << mouseX << ", " << mouseY << ")" << std::endl;
	std::cout << "[OverlayViewport] Viewport: " << screenWidth << "x" << screenHeight << std::endl;
	std::cout << "[OverlayViewport] Camera position: (" << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
	
	for (const auto* obj : objects) {
		if (!obj) continue;
		std::cout << "[OverlayViewport] Object: " << obj->getName() 
				  << " | Selectable: " << (obj->isSelectable() ? "YES" : "NO")
				  << " | Pos: (" << obj->getPosition().x << ", " << obj->getPosition().y << ", " << obj->getPosition().z << ")" << std::endl;
	}
	
	std::cout << "[OverlayViewport] Starting raycast with " << objects.size() << " objects..." << std::endl;
}

void ThreeDObjectSelector::pickUpMesh(int mouseX, int mouseY, int screenWidth, int screenHeight, const glm::mat4 &view, const glm::mat4 &projection, const std::vector<ThreeDObject *> &objects)
{
	if (screenWidth <= 0 || screenHeight <= 0) 
	{
		std::cerr << "[ThreeDObjectSelector] Invalid screen dimensions" << std::endl;
		return;
	}
	
	try 
	{
		// Convert mouse coordinates to OpenGL convention (y=0 at bottom)
		float mouseY_GL = screenHeight - mouseY;
		
		std::cout << " [pickUpMesh] Mouse input: (" << mouseX << ", " << mouseY << ")" << std::endl;
		std::cout << " [pickUpMesh] Mouse GL (after Y-flip): (" << mouseX << ", " << mouseY_GL << ")" << std::endl;
		std::cout << " [pickUpMesh] Screen: " << screenWidth << "x" << screenHeight << std::endl;
		
		glm::vec3 rayStart = glm::unProject(glm::vec3(mouseX, mouseY_GL, 0.0f), view, projection, glm::vec4(0, 0, screenWidth, screenHeight));
		glm::vec3 rayEnd = glm::unProject(glm::vec3(mouseX, mouseY_GL, 1.0f), view, projection, glm::vec4(0, 0, screenWidth, screenHeight));

		glm::vec3 rayDir = glm::normalize(rayEnd - rayStart);
		glm::vec3 rayOrigin = rayStart;
		
		std::cout << " [pickUpMesh] Ray origin: (" << rayOrigin.x << ", " << rayOrigin.y << ", " << rayOrigin.z << ")" << std::endl;
		std::cout << " [pickUpMesh] Ray dir: (" << rayDir.x << ", " << rayDir.y << ", " << rayDir.z << ")" << std::endl;

		float closestDistance = std::numeric_limits<float>::max();
		ThreeDObject *closestObject = nullptr;

		for (auto *obj : objects)
		{
			if (!obj) continue;
			if (!obj->isSelectable()) continue;
			
			try {
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
			catch (const std::exception& e) 
			{
				std::cerr << "[ThreeDObjectSelector] Error processing object: " << e.what() << std::endl;
			}
		}

		selectedObject = closestObject;
	} 
	catch (const std::exception& e) {
		std::cerr << "[ThreeDObjectSelector] Error during pickUpMesh: " << e.what() << std::endl;
	}
}

bool ThreeDObjectSelector::rayIntersectsMesh(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object)
{
	std::cout << " [rayIntersectsMesh] Testing object: " << object.getName() << std::endl;
	
	try
	{
		const Mesh* mesh = dynamic_cast<const Mesh*>(&object);
		
		if (mesh) 
		{
			// Test intersection with actual mesh faces (more precise)
			const auto& faces = mesh->getFaces();
			
			if (faces.empty()) 
			{
				std::cout << " No faces found - using bounding box fallback" << std::endl;
				// Fallback to bounding box if no faces
				return rayIntersectsBoundingBox(rayOrigin, rayDir, object);
			}
			
			std::cout << " Testing " << faces.size() << " faces..." << std::endl;
			
			// Test each face for intersection
			for (const auto* face : faces) 
			{
				if (rayIntersectsFace(rayOrigin, rayDir, object, *face)) {
					std::cout << "      HIT: Ray intersects face!" << std::endl;
					return true;
				}
			}
			
			std::cout << "      MISS: No face intersection" << std::endl;
			return false;
		}
		else {
			// Not a mesh - fallback to bounding box
			std::cout << "      Not a mesh - using bounding box test" << std::endl;
			return rayIntersectsBoundingBox(rayOrigin, rayDir, object);
		}
	} 
	catch (const std::exception& e) 
	{
		std::cerr << "[ThreeDObjectSelector] Error in rayIntersectsMesh: " << e.what() << std::endl;
		return false;
	}
}

bool ThreeDObjectSelector::rayIntersectsBoundingBox(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const ThreeDObject &object)
{
	try {
		glm::mat4 model = object.getModelMatrix();
		
		if (glm::length(glm::vec3(model[0])) < 0.0001f || 
			glm::length(glm::vec3(model[1])) < 0.0001f || 
			glm::length(glm::vec3(model[2])) < 0.0001f) {
			return false;
		}
		
		glm::vec3 center = glm::vec3(model * glm::vec4(0, 0, 0, 1));
		glm::vec3 halfScale = object.getScale() * 0.5f;
		
		// Protect against zero scale
		if (halfScale.x <= 0.0001f || halfScale.y <= 0.0001f || halfScale.z <= 0.0001f) {
			return false;
		}

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

		bool axisTests = testAxis(xAxis, halfScale.x) &&
						 testAxis(yAxis, halfScale.y) &&
						 testAxis(zAxis, halfScale.z);

		// Final check: tMax must be >= 0 (in front of ray origin) and tMin <= tMax
		return axisTests && (tMax >= 0.0f) && (tMin <= tMax);
	} 
	catch (const std::exception& e) 
	{
		std::cerr << "[ThreeDObjectSelector] Error in rayIntersectsMesh: " << e.what() << std::endl;
		return false;
	}
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
	if (verts.size() < 3) return false;  // Changed from 4 to support triangles too

	// Get face vertices in world space
	std::vector<glm::vec3> worldVerts;
	for (size_t i = 0; i < verts.size(); ++i) {
		worldVerts.push_back(glm::vec3(model * glm::vec4(verts[i]->getLocalPosition(), 1.0f)));
	}
	
	// DEBUG: compute min/max bounds of the face vertices (used later for a quick reject)
	glm::vec3 minBounds = worldVerts[0];
	glm::vec3 maxBounds = worldVerts[0];
	for (size_t i = 1; i < worldVerts.size(); ++i) {
		minBounds = glm::min(minBounds, worldVerts[i]);
		maxBounds = glm::max(maxBounds, worldVerts[i]);
	}
	if (verts.size() >= 3) {
		std::cout << "        [Face] Bounds: min(" << minBounds.x << "," << minBounds.y << "," << minBounds.z << ")"
				  << " max(" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")" << std::endl;
	}

	// Compute face normal (using first 3 vertices)
	glm::vec3 p0 = worldVerts[0];
	glm::vec3 p1 = worldVerts[1];
	glm::vec3 p2 = worldVerts[2];
	
	glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
	
	// Check if ray is parallel to face
	float denom = glm::dot(n, rayDir);
	std::cout << "        [Face] Vertices: " << verts.size() 
			  << " | Normal: (" << n.x << ", " << n.y << ", " << n.z << ")"
			  << " | Denom: " << denom << std::endl;
	
	if (std::abs(denom) < 1e-6f) {
		std::cout << "        [Face] SKIP: Ray parallel to face (|denom| < 1e-6)" << std::endl;
		return false;  // Ray parallel to face
	}

	// Compute intersection point with face plane
	float t = glm::dot(n, (p0 - rayOrigin)) / denom;
	std::cout << "        [Face] t=" << t;
	
	if (t < 0.0f) {
		std::cout << " (BEHIND camera - rejected)" << std::endl;
		return false;  // Intersection behind ray origin
	}
	std::cout << " (in front)" << std::endl;

	glm::vec3 P = rayOrigin + t * rayDir;
	std::cout << "        [Face] Intersection P: (" << P.x << ", " << P.y << ", " << P.z << ")" << std::endl;
	std::cout << "        [Face] Face verts: ";
	for (size_t i = 0; i < worldVerts.size(); ++i) {
		std::cout << "p" << i << "(" << worldVerts[i].x << "," << worldVerts[i].y << "," << worldVerts[i].z << ")";
		if (i < worldVerts.size() - 1) std::cout << " ";
	}
	std::cout << std::endl;

	const float boundsEps = 1e-6f;
	if (P.x < minBounds.x - boundsEps || P.x > maxBounds.x + boundsEps ||
		P.y < minBounds.y - boundsEps || P.y > maxBounds.y + boundsEps ||
		P.z < minBounds.z - boundsEps || P.z > maxBounds.z + boundsEps) {
		std::cout << "        [Face] AABB REJECT: point outside face bounds" << std::endl;
		return false;
	}

	// Point-in-triangle test using barycentric coordinates (more robust)
	auto pointInTri = [](const glm::vec3& A, const glm::vec3& B, const glm::vec3& C, const glm::vec3& P) -> bool
	{
		// Compute vectors
		glm::vec3 v0 = C - A;
		glm::vec3 v1 = B - A;
		glm::vec3 v2 = P - A;

		// Compute dot products
		float dot00 = glm::dot(v0, v0);
		float dot01 = glm::dot(v0, v1);
		float dot02 = glm::dot(v0, v2);
		float dot11 = glm::dot(v1, v1);
		float dot12 = glm::dot(v1, v2);

	// Compute barycentric coordinates
	float denom = (dot00 * dot11 - dot01 * dot01);
	if (std::abs(denom) < 1e-10f) return false;  // Degenerate triangle
		
	float invDenom = 1.0f / denom;
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

	// Check if point is in triangle (tight epsilon for pixel-precision)
	const float eps = 1e-6f;
	return (u >= -eps) && (v >= -eps) && (u + v <= 1.0f + eps);
	};

	// Test all possible triangulations of the face
	if (verts.size() == 3) {
		// Triangle face
		bool hit = pointInTri(worldVerts[0], worldVerts[1], worldVerts[2], P);
		std::cout << "        [Face] Triangle test: " << (hit ? "INSIDE" : "OUTSIDE") << std::endl;
		return hit;
	}
	else if (verts.size() == 4) {
		// Quad face - test ALL possible triangulations (there are 4 ways to split a quad into 2 triangles)
		// Diagonal 0-2: triangles (0,1,2) and (0,2,3)
		bool tri1 = pointInTri(worldVerts[0], worldVerts[1], worldVerts[2], P);
		bool tri2 = pointInTri(worldVerts[0], worldVerts[2], worldVerts[3], P);
		
		// Diagonal 1-3: triangles (1,2,3) and (1,3,0)
		bool tri3 = pointInTri(worldVerts[1], worldVerts[2], worldVerts[3], P);
		bool tri4 = pointInTri(worldVerts[1], worldVerts[3], worldVerts[0], P);
		
		std::cout << "        [Face] Quad tests: tri1=" << tri1 << " tri2=" << tri2 
				  << " tri3=" << tri3 << " tri4=" << tri4 << std::endl;
		
		if (tri1 || tri2 || tri3 || tri4) {
			return true;
		}
		return false;  // Point not inside quad
	}
	else 
	{
		// N-gon: test fan triangulation from first vertex
		std::cout << "        [Face] N-gon with " << verts.size() << " vertices" << std::endl;
		for (size_t i = 1; i < verts.size() - 1; ++i) 
		{
			if (pointInTri(worldVerts[0], worldVerts[i], worldVerts[i+1], P)) 
			{
				std::cout << "        [Face] N-gon triangle " << i << " HIT" << std::endl;
				return true;
			}
		}
		std::cout << "        [Face] N-gon ALL triangles MISS" << std::endl;
		return false;
	}
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
