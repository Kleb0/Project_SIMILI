#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <iostream>
#include <list>

class ThreeDObject
{
public:
    ThreeDObject();
    virtual ~ThreeDObject();

    virtual void initialize() = 0;
    virtual void render(const glm::mat4 &viewProj) = 0;
    virtual void destroy() {}

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getRotation() const { return glm::degrees(glm::eulerAngles(rotation)); }
    glm::vec3 getScale() const { return _scale; }

    void setPosition(const glm::vec3 &pos) { position = pos; }
    void setRotation(const glm::vec3 &eulerDegrees) { rotation = glm::quat(glm::radians(eulerDegrees)); }
    void setScale(const glm::vec3 &scl) { _scale = scl; }

    void translate(const glm::vec3 &newPosition);
    void rotate(const glm::vec3 &newEulerRotationDegrees);
    void scale(const glm::vec3 &newScale);

    glm::mat4 getModelMatrix() const;
    void setModelMatrix(const glm::mat4 &matrix);
    glm::vec3 getCenter() const;

    void setGlobalModelMatrix(const glm::mat4 &newGlobalMatrix);

    glm::mat4 getGlobalModelMatrix() const;

    void setSelected(bool selected) { isCurrentlySelected = selected; }
    bool getSelected() const { return isCurrentlySelected; }
    virtual bool isSelectable() const { return true; }

    void setName(const std::string &newName) { name = newName; }
    virtual std::string getName() const { return name; }

    void setParent(ThreeDObject *newParent);
    ThreeDObject *getParent() const { return parent; }
    void removeParent() { parent = nullptr; }

    void addChild(ThreeDObject *child);
    void removeChild(ThreeDObject *child);
    const std::list<ThreeDObject *> &getChildren() const;
    std::list<ThreeDObject*>& getChildren() { return children; }

    bool isDescendantOf(ThreeDObject *potentialAncestor) const;
    bool isParented = false;
    bool canBeParented = true;
    bool canHaveChildren = true;
    virtual bool isDummy() const { return false; }
    virtual bool isInspectable() const { return true; }

    void setOrigin (const glm::vec3& origin);
    glm::vec3 getOrigin() const;
    

    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 _scale = glm::vec3(1.0f);

    ThreeDObject *parent = nullptr;
    std::list<ThreeDObject *> children;

    
    bool expanded = true;

    void setSlot(int index) { slotIndex = index; }
    int getSlot() const { return slotIndex; }

    void setMovedSlotIndex(int index) { movedSlotIndex = index; }
    int getMovedSlotIndex() const { return movedSlotIndex; }
    

protected:

    int movedSlotIndex = -1;
    int slotIndex  = -1;
    bool isCurrentlySelected = false;
    std::string name = "Unnamed";
    glm::vec3 origin = glm::vec3(0.0f); 
};
