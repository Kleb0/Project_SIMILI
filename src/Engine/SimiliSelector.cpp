#include "Engine/SimiliSelector.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include "Engine/ErrorBox.hpp"


void SimiliSelector::setWindow(ThreeDWindow* win) {
    window = win;
}

void SimiliSelector::setScene(ThreeDScene* scn) 
{
    scene = scn;
}

void SimiliSelector::setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects)
{
    selectedObjects = objects;

    for (ThreeDObject* obj : scene->getObjectsRef())
        obj->setSelected(false);

    for (ThreeDObject* obj : selectedObjects)
        obj->setSelected(true);

    calculateCenterOfSelection();
    window->lockSelectionOnce(); 
}

void SimiliSelector::selectMultipleObjects(const std::list<ThreeDObject*>& objects)
{
    selectedObjects = objects;

    for (ThreeDObject* obj : scene->getObjectsRef())
        obj->setSelected(false);

    for (ThreeDObject* obj : selectedObjects)
        obj->setSelected(true);
}

void SimiliSelector::toggleMultipleSelection(ThreeDObject* object)
{
    auto it = std::find(selectedObjects.begin(), selectedObjects.end(), object);
    if (it != selectedObjects.end()) 
    {
        selectedObjects.erase(it);
        object->setSelected(false);
    } 
    else 
    {
        selectedObjects.push_back(object);
        object->setSelected(true);
    }
}

void SimiliSelector::externalSelect(ThreeDObject* object)
{
    for (ThreeDObject* obj : scene->getObjectsRef())
        obj->setSelected(false);

    if (object) 
    {
        window->getSelector().select(object); 
        object->setSelected(true);
    } 
    else 
    {
        window->getSelector().clearTarget();
    }

    if (window->getHierarchy())
        window->getHierarchy()->selectFromThreeDWindow();
}

void SimiliSelector::calculateCenterOfSelection()
{
    if (selectedObjects.empty()) {
        centerOfSelection = glm::vec3(0.0f);
        return;
    }

    glm::vec3 sum(0.0f);
    for (const ThreeDObject* obj : selectedObjects) {
        sum += obj->getPosition();
    }

    centerOfSelection = sum / static_cast<float>(selectedObjects.size());
}

const std::list<ThreeDObject*>& SimiliSelector::getSelectedObjects() const
{
    return selectedObjects;
}

glm::vec3 SimiliSelector::getCenterOfSelection() const
{
    return centerOfSelection;
}

void SimiliSelector::clearSelection()
{

    const auto& objects = scene->getObjectsRef();
    int index = 0;

    for (ThreeDObject* obj : objects)
    {

        if (!obj)
        {
            ++index;
            continue;
        }

        obj->setSelected(false);  

        ++index;
    }

    selectedObjects.clear();
    centerOfSelection = glm::vec3(0.0f);

}

