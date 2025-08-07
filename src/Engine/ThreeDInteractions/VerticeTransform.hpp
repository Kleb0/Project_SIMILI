#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

class OpenGLContext;
class Vertice;

namespace VerticeTransform
{
    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context,
                                 Vertice* vertice, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

    void manipulateVertice(OpenGLContext* context, Vertice* selectedVertice,
                            const ImVec2& oglChildPos, const ImVec2& oglChildSize,
                            bool& wasUsingGizmoLastFrame);
}
