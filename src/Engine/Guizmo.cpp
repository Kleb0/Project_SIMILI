#define GLM_ENABLE_EXPERIMENTAL
#include "Engine/Guizmo.hpp"
#include "WorldObjects/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include <glm/gtc/type_ptr.hpp>


glm::mat4 Guizmo::renderGizmoForObject(const std::list<ThreeDObject*>& objects, ImGuizmo::OPERATION operation, const glm::mat4& view,
const glm::mat4& proj, ImVec2 oglChildPos, ImVec2 oglChildSize)
{
    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(oglChildPos.x, oglChildPos.y, oglChildSize.x, oglChildSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);

    glm::vec3 center(0.0f);
    glm::vec3 averageScale(0.0f);
    std::vector<glm::quat> rotations;

    for (auto* obj : objects)
    {
        center += obj->getPosition();
        averageScale += obj->getScale();
        rotations.push_back(obj->rotation);
    }

    center /= static_cast<float>(objects.size());
    averageScale /= static_cast<float>(objects.size());

    glm::vec4 cumulative(0.0f);
    for (const auto& q : rotations)
    {
        glm::quat aligned = glm::dot(q, rotations[0]) < 0.0f ? -q : q;
        cumulative += glm::vec4(aligned.x, aligned.y, aligned.z, aligned.w);
    }

    cumulative = glm::normalize(cumulative);
    glm::quat avgRotation = glm::quat(cumulative.w, cumulative.x, cumulative.y, cumulative.z);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model *= glm::toMat4(glm::normalize(avgRotation));
    model = glm::scale(model, averageScale);

    return model;
}

glm::mat4 Guizmo::renderGizmoForVertices(const std::list<Vertice*>& vertices,
                                         ImGuizmo::OPERATION operation,
                                         const glm::mat4& view, const glm::mat4& proj,
                                         ImVec2 oglChildPos, ImVec2 oglChildSize)
{
    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(oglChildPos.x, oglChildPos.y, oglChildSize.x, oglChildSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);

    glm::vec3 center(0.0f);
    int count = 0;
    for (auto* v : vertices)
    {
        if (!v) continue;
        ThreeDObject* parent = v->getMeshParent();
        glm::vec3 lp = v->getPosition();
        glm::vec4 wp = parent ? (parent->getModelMatrix() * glm::vec4(lp, 1.0f)) : glm::vec4(lp, 1.0f);
        center += glm::vec3(wp);
        ++count;
    }
    if (count > 0) center /= static_cast<float>(count);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    return model;

    // the model needs to be updated once the vertices have been manipulated
}