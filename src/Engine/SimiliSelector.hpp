#pragma once

#include <list>
#include <glm/glm.hpp>

class ThreeDObject;
class ThreeDWindow;
class OpenGLContext;
class ObjectInspector;
class ThreeDScene;

class SimiliSelector 
{

public:
    SimiliSelector() = default;

    void setWindow(ThreeDWindow* win);
    void setScene(ThreeDScene* scene);

    void setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects);
    void selectMultipleObjects(const std::list<ThreeDObject*>& objects);
    void toggleMultipleSelection(ThreeDObject* object);
    void externalSelect(ThreeDObject* object);

    const std::list<ThreeDObject*>& getSelectedObjects() const;
    glm::vec3 getCenterOfSelection() const;

    void clearSelection();

private:
    void calculateCenterOfSelection();

    std::list<ThreeDObject*> selectedObjects;
    glm::vec3 centerOfSelection = glm::vec3(0.0f);
    ThreeDWindow* window = nullptr;
    ThreeDScene* scene = nullptr;

};
