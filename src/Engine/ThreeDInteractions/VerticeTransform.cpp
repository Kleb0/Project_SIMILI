#include "Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/OpenGLScene/ThreeDScene.hpp"
#include "Engine/Guizmo.hpp"
#include "SIMILI_Frontend/viewportLogic/Keymanagement/KeyManager.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>

#include <unordered_set>
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"

namespace VerticeTransform
{

	static inline bool isIdentity(const glm::mat4& m) {
		static const glm::mat4 I(1.0f);
		return glm::all(glm::epsilonEqual(glm::vec4(m[0]), glm::vec4(I[0]), 1e-6f)) &&
			glm::all(glm::epsilonEqual(glm::vec4(m[1]), glm::vec4(I[1]), 1e-6f)) &&
			glm::all(glm::epsilonEqual(glm::vec4(m[2]), glm::vec4(I[2]), 1e-6f)) &&
			glm::all(glm::epsilonEqual(glm::vec4(m[3]), glm::vec4(I[3]), 1e-6f));
	}


	glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, ThreeDScene* scene, const std::list<Vertice*>& vertices,
	const ImVec2& oglChildPos, const ImVec2& oglChildSize)
	{
			glm::mat4 view = scene->getViewMatrix();
			glm::mat4 proj = scene->getProjectionMatrix();

			glm::mat4 model = Guizmo::renderGizmoForVertices(vertices, op, view, proj, oglChildPos, oglChildSize);
			return model;
	}

	void manipulateVertices(ThreeDScene* scene, const std::list<Vertice*>& selectedVertices,
	const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame,
	const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
	{
		static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
		
		auto& keyManager = SIMILI::Input::KeyManager::getInstance();
		auto* inputSystem = keyManager.getInputSystem();
		
		if (inputSystem)
		{
			inputSystem->pollKeyStates();
			
			const auto* wKeyState = inputSystem->getKeyState('W');
			const auto* rKeyState = inputSystem->getKeyState('R');
			const auto* sKeyState = inputSystem->getKeyState('S');
			
			if (wKeyState && wKeyState->isFirstPress)
			{
				currentGizmoOperation = ImGuizmo::TRANSLATE;
			}
			if (rKeyState && rKeyState->isFirstPress)
			{
				currentGizmoOperation = ImGuizmo::ROTATE;
			}
			if (sKeyState && sKeyState->isFirstPress)
			{
				currentGizmoOperation = ImGuizmo::SCALE;
			}
		}
		
		if (selectedVertices.empty()) return;

		glm::mat4 view = viewMatrix;
		glm::mat4 proj = projectionMatrix;

		static glm::mat4 dummyMatrix = glm::mat4(1.0f);
		static glm::mat4 startMatrix = glm::mat4(1.0f);
		static glm::mat4 prevMatrix = glm::mat4(1.0f);
		static size_t previousSetHash = 0;
		static bool gizmoActive = false;
		static glm::vec3 lockedCenter = glm::vec3(0.0f);
		static ThreeDObject* lockedParent = nullptr;

		static std::unordered_map<Vertice*, glm::vec3> initialLocalPositions;
		static glm::mat4 totalAccumDelta = glm::mat4(1.0f);

		auto hashSet = [&]() -> size_t {
			size_t h = 1469598103934665603ull;
			for (auto* v : selectedVertices) {
				size_t x = reinterpret_cast<size_t>(v);
				h ^= x; h *= 1099511628211ull;
			}
			h ^= selectedVertices.size();
			return h;
		};

		size_t currentHash = hashSet();
		bool usingGizmo = ImGuizmo::IsUsing();
		bool selectionChanged = (currentHash != previousSetHash);

		glm::vec3 center = lockedCenter;
		ThreeDObject* firstParent = lockedParent;
		
		if (!gizmoActive)
		{
			center = glm::vec3(0.0f);
			int count = 0;
			firstParent = nullptr;
			
			for (auto* v : selectedVertices) 
			{
				if (!v) continue;
				
				if (!firstParent) {
					firstParent = v->getMeshParent();
				}
				
				center += v->getPosition();
				++count;
			}
			if (count > 0) center /= static_cast<float>(count);
		}

		if (selectionChanged || (!usingGizmo && !gizmoActive)) 
		{
			dummyMatrix = glm::translate(glm::mat4(1.0f), center);
			startMatrix = dummyMatrix;
			prevMatrix = dummyMatrix;
			gizmoActive = false;
			lockedCenter = center;
			lockedParent = firstParent;
			initialLocalPositions.clear();
			totalAccumDelta = glm::mat4(1.0f);
			
			if (selectionChanged) 
			{
				previousSetHash = currentHash;
			}
		}

		glm::mat4 gizmoMatrix = Guizmo::renderGizmoForVertices(selectedVertices, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);
		
		if (!gizmoActive)
		{
			dummyMatrix = gizmoMatrix;
		}

		if (usingGizmo && !gizmoActive)
		{
			gizmoActive = true;
			lockedCenter = center;
			lockedParent = firstParent;
			startMatrix = dummyMatrix;
			prevMatrix = dummyMatrix;
			initialLocalPositions.clear();
			totalAccumDelta = glm::mat4(1.0f);
			
			std::unordered_set<Vertice*> uniqueVerts;
			for (auto* v : selectedVertices) if (v) uniqueVerts.insert(v);
			
			for (auto* v : uniqueVerts)
			{
				if (v) {
					initialLocalPositions[v] = v->getLocalPosition();
				}
			}
		}

		// Manipulate the gizmo
		const bool Manipulated = ImGuizmo::Manipulate(
			glm::value_ptr(view), 
			glm::value_ptr(proj),
			currentGizmoOperation, 
			ImGuizmo::WORLD, 
			glm::value_ptr(dummyMatrix)
		);

		// During drag: apply transformation from INITIAL positions
		if (usingGizmo && Manipulated && lockedParent)
		{
			// Calculate TOTAL delta from start of drag
			glm::mat4 totalDelta = dummyMatrix * glm::inverse(startMatrix);
			
			// Check if delta is not identity
			if (!isIdentity(totalDelta))
			{
				glm::mat4 parentMat = lockedParent->getModelMatrix();
				glm::mat4 parentInv = glm::inverse(parentMat);
				
				// Apply total transformation from initial positions
				for (auto& pair : initialLocalPositions)
				{
					Vertice* v = pair.first;
					glm::vec3 initialLocalPos = pair.second;
					
					if (!v) continue;
					
					// Transform initial local position to world space
					glm::vec3 initialWorldPos = glm::vec3(parentMat * glm::vec4(initialLocalPos, 1.0f));
					
					// Apply total transformation in world space
					glm::vec4 transformedWorld = totalDelta * glm::vec4(initialWorldPos, 1.0f);
					
					// Transform back to local space
					glm::vec3 newLocalPos = glm::vec3(parentInv * transformedWorld);
					
					v->setLocalPosition(newLocalPos);
					v->setPosition(glm::vec3(transformedWorld));
				}
				
				// Store for DNA tracking
				totalAccumDelta = totalDelta;
			}
			
			prevMatrix = dummyMatrix;
		}

		// End of drag: track changes in DNA
		if (!usingGizmo && gizmoActive)
		{
			Mesh* parentMesh = nullptr;
			std::vector<Vertice*> vertsSnapshot;
			
			if (!selectedVertices.empty()) 
			{
				if (auto* any = selectedVertices.front()) 
				{
					if (auto* p = any->getMeshParent()) 
					{
						parentMesh = dynamic_cast<Mesh*>(p);
					}
				}
			}

			// Collect vertices for DNA tracking
			for (auto& pair : initialLocalPositions)
			{
				if (pair.first) {
					vertsSnapshot.push_back(pair.first);
				}
			}

			if (parentMesh && !isIdentity(totalAccumDelta)) 
			{
				if (auto* dna = parentMesh->getMeshDNA()) 
				{
					std::cout << "Tracking Vertice modification in Mesh DNA." << std::endl;
					dna->trackVerticeModify(totalAccumDelta, vertsSnapshot);
				}
			}

			gizmoActive = false;
			initialLocalPositions.clear();
			totalAccumDelta = glm::mat4(1.0f);
		}

		wasUsingGizmoLastFrame = usingGizmo;
	}
}