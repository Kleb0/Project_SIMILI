#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <vector>

#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "../SIMILI_Frontend/viewportLogic/UIPanels/WorkSpace.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Face.hpp" 
#include "WorldObjects/Basic/Edge.hpp"

class ThreeDObject;
class Vertice;
class Face;
class Edge;
class OpenGLContext;

namespace SIMILI { namespace Frontend { class WorkSpace; } }

class Guizmo
{
public:
    static glm::mat4 renderGizmoForObject(const std::list<ThreeDObject*>& objects, ImGuizmo::OPERATION operation,
        const glm::mat4& view, const glm::mat4& proj, ImVec2 oglChildPos, ImVec2 oglChildSize);
    
    static glm::mat4 renderGizmoForVertices(const std::list<Vertice*>& vertices,
        ImGuizmo::OPERATION operation, const glm::mat4& view, const glm::mat4& proj,
        ImVec2 oglChildPos, ImVec2 oglChildSize);
    
    static glm::mat4 renderGizmoForFaces(const std::list<Face*>& faces,
        ImGuizmo::OPERATION operation, const glm::mat4& view, const glm::mat4& proj,
        ImVec2 oglChildPos, ImVec2 oglChildSize);
    
    static glm::mat4 renderGizmoForEdges(const std::list<Edge*>& edges,
        ImGuizmo::OPERATION operation, const glm::mat4& view, const glm::mat4& proj,
        ImVec2 oglChildPos, ImVec2 oglChildSize);
    
    static bool manipulateGizmo(const glm::mat4& modelMatrix, const glm::mat4& viewMatrix, 
        const glm::mat4& projectionMatrix, ImGuizmo::OPERATION operation,
        ImGuizmo::MODE mode, ImVec2 viewportPos,
        ImVec2 viewportSize, glm::mat4& outputModelMatrix);
    
    static void getWorkspaceViewport(const SIMILI::Frontend::WorkSpace& workspace,
        ImVec2& pos, ImVec2& size);
};

