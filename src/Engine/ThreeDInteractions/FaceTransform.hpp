#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

class OpenGLContext;
class Face;

namespace FaceTransform
{
   glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context,
   const std::list<Face*>& faces, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

   void manipulateFaces(OpenGLContext* context, const std::list<Face*>& selectedFaces,
   const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, bool bakeToVertices = true);

}
