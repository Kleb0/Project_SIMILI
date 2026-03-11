#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

class VKScene;
class Vertice;

namespace VerticeTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, VKScene * scene,
   const std::list<Vertice*>& vertices, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateVertices(VKScene* scene, const std::list<Vertice*>& selectedVertices,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame,
   const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

}
