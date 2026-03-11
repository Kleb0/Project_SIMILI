	// Displays the edge loop side edges if toggled   
#include "Engine/ThreeDInteractions/EdgeTransform.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"

#include "Engine/VulkanScene/VKScene.Hpp"
// #include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "Engine/Guizmo.hpp"
#include "Engine/MeshEdit/EdgeLoop.hpp"
#include "SIMILI_Frontend/viewportLogic/Keymanagement/KeyManager.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <cmath>

namespace EdgeTransform
{

	static bool isIdentity(const glm::mat4& m, float eps = 1e-6f)
	{
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				float expected = (i == j) ? 1.0f : 0.0f;
				if (fabs(m[i][j] - expected) > eps) {
					return false;
				}
			}
		}
		return true;
	}

	glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, VKScene* scene,
	const std::list<Edge*>& edges,const ImVec2& oglChildPos, const ImVec2& oglChildSize)
	{
		glm::mat4 view = scene->getViewMatrix();
		glm::mat4 proj = scene->getProjectionMatrix();

		glm::mat4 model = Guizmo::renderGizmoForEdges(edges, op, view, proj, oglChildPos, oglChildSize);
		return model;
	}

	void manipulateEdges(VKScene* scene, std::list<Edge*>& selectedEdges,
	const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, ThreeDWindow* threeDWindow,
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
		
		// Early return if no edges selected - mode switching still works above
		if (selectedEdges.empty()) return;


		// -------- Edge Loop Side Edges Display (toggle) ----------
		EnableEdgeLoop(scene, selectedEdges, oglChildPos, oglChildSize, threeDWindow);
		// -------- End of Edge Loop Display ----------


		glm::mat4 view = viewMatrix;
		glm::mat4 proj = projectionMatrix;

		static glm::mat4 dummyMatrix = glm::mat4(1.0f);
		static glm::mat4 startMatrix = glm::mat4(1.0f);
		static glm::mat4 prevMatrix = glm::mat4(1.0f);
		static size_t previousSetHash = 0;
		static bool gizmoActive = false;
		static glm::vec3 lockedCenter = glm::vec3(0.0f);  // CRITICAL: Lock center during drag
		static ThreeDObject* lockedParent = nullptr;

		static std::unordered_map<Vertice*, glm::vec3> initialLocalPositions;
		static glm::mat4 totalAccumDelta = glm::mat4(1.0f);

		auto hashSet = [&]() -> size_t {
			size_t h = 1469598103934665603ull;
			for (auto* e : selectedEdges) {
				size_t x = reinterpret_cast<size_t>(e);
				h ^= x; h *= 1099511628211ull;
			}
			h ^= selectedEdges.size();
			return h;
		};

		size_t currentHash = hashSet();
		bool usingGizmo = ImGuizmo::IsUsing();
		bool selectionChanged = (currentHash != previousSetHash);

		// Calculate center of selected edges in world space (only when NOT dragging)
		glm::vec3 center = lockedCenter;
		ThreeDObject* firstParent = lockedParent;
		
		if (!gizmoActive)
		{
			center = glm::vec3(0.0f);
			int count = 0;
			firstParent = nullptr;
			
			for (auto* e : selectedEdges)
			{
				if (!e) continue;
				Vertice* a = e->getStart();
				Vertice* b = e->getEnd();
				if (!a || !b) continue;

				if (!firstParent) {
					firstParent = a->getMeshParent() ? a->getMeshParent() : b->getMeshParent();
				}

				ThreeDObject* parent = a->getMeshParent() ? a->getMeshParent() : b->getMeshParent();
				glm::mat4 parentMat = parent ? parent->getModelMatrix() : glm::mat4(1.0f);

				glm::vec3 wa = glm::vec3(parentMat * glm::vec4(a->getLocalPosition(), 1.0f));
				glm::vec3 wb = glm::vec3(parentMat * glm::vec4(b->getLocalPosition(), 1.0f));

				center += 0.5f * (wa + wb);
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

		// CRITICAL: Always render the gizmo, not just when inactive
		// This returns the updated dummy matrix which we use for manipulation
		glm::mat4 gizmoMatrix = Guizmo::renderGizmoForEdges(selectedEdges, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);
		
		// Only update dummyMatrix if we're not actively dragging
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
			for (auto* e : selectedEdges)
			{
				if (!e) continue;
				Vertice* a = e->getStart();
				Vertice* b = e->getEnd();
				if (a) uniqueVerts.insert(a);
				if (b) uniqueVerts.insert(b);
			}
			
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
		
		if (!usingGizmo && gizmoActive)
		{
			// Find parent mesh for DNA tracking
			Mesh* parentMesh = nullptr;
			std::vector<Vertice*> vertsSnapshot;
			
			if (!selectedEdges.empty()) 
			{
				Edge* any = *selectedEdges.begin();
				if (any) 
				{
					Vertice* s = any->getStart();
					Vertice* e = any->getEnd();
					ThreeDObject* p = s && s->getMeshParent() ? s->getMeshParent() : (e ? e->getMeshParent() : nullptr);
					parentMesh = p ? dynamic_cast<Mesh*>(p) : nullptr;
				}
			}

			// Collect vertices for DNA tracking
			for (auto& pair : initialLocalPositions)
			{
				if (pair.first) 
				{
					vertsSnapshot.push_back(pair.first);
				}
			}

			// Track in DNA if we have a parent mesh and non-identity transformation
			if (parentMesh && !isIdentity(totalAccumDelta)) 
			{
				if (auto* dna = parentMesh->getMeshDNA()) 
				{
					dna->trackEdgeModify(totalAccumDelta, vertsSnapshot);
				}
			}

			// Clean up
			gizmoActive = false;
			initialLocalPositions.clear();
			totalAccumDelta = glm::mat4(1.0f);
		}
		
		wasUsingGizmoLastFrame = usingGizmo;
	}       


	void EnableEdgeLoop(VKScene* scene, std::list<Edge*>& selectedEdges, const ImVec2& oglChildPos, 
	const ImVec2& oglChildSize, ThreeDWindow* window)
	{
		static bool showEdgeLoop = false;
		static bool prevCtrlLeft = false;
		bool ctrlLeftPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
		bool ctrlLeftJustPressed = ctrlLeftPressed && !prevCtrlLeft;
		prevCtrlLeft = ctrlLeftPressed;

		if (ctrlLeftJustPressed && selectedEdges.size() == 1) 
		{
			showEdgeLoop = !showEdgeLoop;
			// if (window) {
			//     // window->isEdgeLoopActive = showEdgeLoop;
			// }
		}

		if (showEdgeLoop && selectedEdges.size() == 1)
		{
			Edge* selected = selectedEdges.front();
			Vertice* a = selected->getStart();
			Vertice* b = selected->getEnd();
			ThreeDObject* parent = a && a->getMeshParent() ? a->getMeshParent() : (b ? b->getMeshParent() : nullptr);
			Mesh* mesh = parent ? dynamic_cast<Mesh*>(parent) : nullptr;

			if (mesh && a && selected)
			{
				std::vector<Edge*> loop = MeshEdit::FindLoop(a, selected, mesh, scene, oglChildPos, oglChildSize);
				// if (window) {
				//     window->isEdgeLoopActive = true;
				// }
			}           
		}

	}
}
