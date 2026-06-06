#pragma once
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <list>

class VKScene;
class Face;

namespace FaceTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, VKScene* scene,
   const std::list<Face*>& faces, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateFaces(VKScene* scene, std::list<Face*>& selectedFaces,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, bool bakeToVertices,
   const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

   Face* extrudeSelectedFace(std::list<Face*>& selectedFaces, float distance);
}
