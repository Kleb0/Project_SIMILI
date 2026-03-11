#include "SceneObjectContainer.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"

SceneObjectContainer::SceneObjectContainer()
{
}

SceneObjectContainer::~SceneObjectContainer()
{
    clearObjects();
}

void SceneObjectContainer::addObject(ThreeDObject* object)
{
    if (!object) {
        return;
    }
    
    if (containsObject(object)) {
        return;
    }
    
    objects.push_back(object);
}

bool SceneObjectContainer::removeObject(ThreeDObject* object)
{
    if (!object) {
        return false;
    }
    
    auto it = std::find(objects.begin(), objects.end(), object);
    if (it != objects.end()) {
        objects.erase(it);
        return true;
    }
    
    return false;
}

bool SceneObjectContainer::containsObject(const ThreeDObject* obj) const
{
    return std::find(objects.begin(), objects.end(), obj) != objects.end();
}

void SceneObjectContainer::clearObjects()
{
    objects.clear();
}