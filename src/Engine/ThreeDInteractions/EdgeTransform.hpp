#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

class OpenGLContext;
class Edge;

namespace EdgeTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context,
   const std::list<Edge*>& edges, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateEdges(OpenGLContext* context, std::list<Edge*>& selectedEdges,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame);

}
