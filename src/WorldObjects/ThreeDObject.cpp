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

void ThreeDObject::setParent(ThreeDObject *newParent)
{
    if (!newParent || newParent == this)
        return;

    if (isDescendantOf(newParent) || newParent->isDescendantOf(this))
        return;

    if (parent)
        parent->removeChild(this);

    parent = newParent;

    if (parent)
        parent->addChild(this);
}
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

void ThreeDObject::addChild(ThreeDObject *child)
{
    if (!child || child == this)
        return;

    if (std::find(children.begin(), children.end(), child) == children.end())
    {
        children.push_back(child);
    }
}

void ThreeDObject::removeChild(ThreeDObject *child)
{
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end())
    {
        children.erase(it);
        child->setParent(nullptr);
    }
}

const std::list<ThreeDObject *> &ThreeDObject::getChildren() const
{
    return children;
}

bool ThreeDObject::isDescendantOf(ThreeDObject *potentialAncestor) const
{
    const ThreeDObject *current = this->parent;
    while (current)
    {
        if (current == potentialAncestor)
            return true;
        current = current->parent;
    }
    return false;
}