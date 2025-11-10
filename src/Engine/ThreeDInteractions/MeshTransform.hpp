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
class ThreeDScene;
class ThreeDMode;
class Vertice_Mode;
class Normal_Mode;

namespace MeshTransform
{
    void manipulateChildrens(ThreeDObject* parent, const glm::mat4& delta);

    void applyGizmoTransformation(ThreeDScene* scene, const glm::mat4& delta, const std::list<ThreeDObject*>& selectedObjects, ImGuizmo::OPERATION op);

    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, ThreeDScene* scene, 
    const std::list<ThreeDObject*>& selectedObjects, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

    void manipulateMesh(ThreeDScene* scene, const std::list<ThreeDObject*>& selectedObjects,
    const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame,
    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

    void trackMeshTransformOnRelease(ThreeDScene* scene, const std::list<ThreeDObject*>& selectedObjects, const glm::mat4& totalDelta, ImGuizmo::OPERATION op);

    void scaleCleanup(ThreeDObject* obj, const glm::mat4& delta);

    void outputTransform(const std::list<ThreeDObject*>& selectedObjects, const std::vector<glm::vec3>& initialPositions,
    bool hasRotatedOnce, bool& didPrintRotationAfterScale);

}