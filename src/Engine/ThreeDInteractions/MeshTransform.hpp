#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <list>
#include <vector>


class ThreeDObject;
class Vertice;
class OpenGLContext;
class ThreeDMode;
class Vertice_Mode;
class Normal_Mode;

namespace MeshTransform
{
    void manipulateChildrens(ThreeDObject* parent, const glm::mat4& delta);

    void applyGizmoTransformation(const glm::mat4& delta, const std::list<ThreeDObject*>& selectedObjects);

    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context, 
    const std::list<ThreeDObject*>& selectedObjects, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

    void manipulateMesh(OpenGLContext* context, const std::list<ThreeDObject*>& selectedObjects,
    const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame);
}
