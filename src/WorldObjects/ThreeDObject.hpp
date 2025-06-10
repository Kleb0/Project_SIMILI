#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>

class ThreeDObject
{
public:
    ThreeDObject();
    virtual ~ThreeDObject();

    virtual void initialize() = 0;
    virtual void render(const glm::mat4 &viewProj) = 0;
    virtual void destroy() {};

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getRotation() const { return glm::degrees(glm::eulerAngles(rotation)); }
    glm::vec3 getScale() const { return _scale; }

    void setPosition(const glm::vec3 &pos) { position = pos; }
    void setRotation(const glm::vec3 &eulerDegrees) { rotation = glm::quat(glm::radians(eulerDegrees)); }
    void setScale(const glm::vec3 &scl) { _scale = scl; }

    void translate(const glm::vec3 &newPosition);
    void rotate(const glm::vec3 &newEulerRotationDegrees);
    void scale(const glm::vec3 &newScale);

    glm::vec3 getCenter() const;
    void setModelMatrix(const glm::mat4 &matrix);
    glm::mat4 getModelMatrix() const;

    void setSelected(bool selected) { isCurrentlySelected = selected; }
    bool getSelected() const { return isCurrentlySelected; }
    virtual bool isSelectable() const { return true; }

    void setName(const std::string &newName) { name = newName; }

    std::string getName() const
    {
        return name;
    }

protected:
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 _scale = glm::vec3(1.0f);
    bool isCurrentlySelected = false;
    std::string name = "Unnamed";
};
