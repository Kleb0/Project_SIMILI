#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <list>

class ThreeDScene;
class Face;

namespace FaceTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, ThreeDScene* scene,
   const std::list<Face*>& faces, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateFaces(ThreeDScene* scene, std::list<Face*>& selectedFaces,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, bool bakeToVertices = true);

   Face* extrudeSelectedFace(std::list<Face*>& selectedFaces, float distance);
}
