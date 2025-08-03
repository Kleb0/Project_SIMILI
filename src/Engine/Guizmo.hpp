#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <imgui.h>
#include <ImGuizmo.h>


class ThreeDObject;
class Vertice;
class OpenGLContext;

class Guizmo
{
public:
    static glm::mat4 renderGizmoForObject(const std::list<ThreeDObject*>& objects, ImGuizmo::OPERATION operation,
    const glm::mat4& view, const glm::mat4& proj, ImVec2 oglChildPos, ImVec2 oglChildSize);

    static glm::mat4 renderGizmoForVertice(Vertice* vertice, ImGuizmo::OPERATION operation, 
    const glm::mat4& view,const glm::mat4& proj, ImVec2 oglChildPos, ImVec2 oglChildSize);
};
