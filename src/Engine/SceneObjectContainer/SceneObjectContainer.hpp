#pragma once

#include <list>
#include <algorithm>

class ThreeDObject;

class SceneObjectContainer
{
public:
    SceneObjectContainer();
    ~SceneObjectContainer();

    void addObject(ThreeDObject* object);
    bool removeObject(ThreeDObject* object);
    bool containsObject(const ThreeDObject* obj) const;
    
    std::list<ThreeDObject*>& getObjectsRef() { return objects; }
    const std::list<ThreeDObject*>& getObjects() const { return objects; }
    
    void clearObjects();
    size_t getObjectCount() const { return objects.size(); }

private:
    std::list<ThreeDObject*> objects;
};