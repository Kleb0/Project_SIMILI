#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

class VKScene;
class ThreeDWindow;
class Edge;

namespace EdgeTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, VKScene* scene,
   const std::list<Edge*>& edges, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateEdges(VKScene* scene, std::list<Edge*>& selectedEdges,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, ThreeDWindow* threeDWindow,
   const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

   void EnableEdgeLoop(VKScene* scene, std::list<Edge*>& selectedEdges, const ImVec2& oglChildPos, const ImVec2& oglChildSize, ThreeDWindow* window);

}
