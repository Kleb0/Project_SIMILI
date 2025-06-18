#include "WorldObjects/ThreedObject.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtx/matrix_decompose.hpp>

ThreeDObject::ThreeDObject() {}
ThreeDObject::~ThreeDObject() {}

void ThreeDObject::translate(const glm::vec3 &newPosition) { position = newPosition; }
void ThreeDObject::rotate(const glm::vec3 &newEulerRotationDegrees) { rotation = glm::quat(glm::radians(newEulerRotationDegrees)); }
void ThreeDObject::scale(const glm::vec3 &newScale) { _scale = newScale; }

glm::vec3 ThreeDObject::getCenter() const { return position; }

void ThreeDObject::setModelMatrix(const glm::mat4 &matrix)
{
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::vec3 positionTmp;
    glm::quat rotationTmp;
    glm::vec3 scaleTmp;

    if (glm::decompose(matrix, scaleTmp, rotationTmp, positionTmp, skew, perspective))
    {
        _scale = scaleTmp;
        rotation = glm::normalize(rotationTmp);
        position = positionTmp;
    }
}
glm::mat4 ThreeDObject::getModelMatrix() const
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model *= glm::toMat4(rotation);
    model = glm::scale(model, _scale);
    return model;
}
