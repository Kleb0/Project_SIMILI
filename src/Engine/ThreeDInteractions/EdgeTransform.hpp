#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

class ThreeDScene;
class Edge;

namespace EdgeTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, ThreeDScene* scene,
   const std::list<Edge*>& edges, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateEdges(ThreeDScene* scene, std::list<Edge*>& selectedEdges,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame);

}
