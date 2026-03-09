#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <list>

class ThreeDObject;

class VKContext
{
public:
    VKContext();
    ~VKContext();

    void initialize();
    void resize(int w, int h);

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    std::string getContextID() const { return contextID; }

    std::list<ThreeDObject*>& getObjectsRef() { return objects; }
    void addObject(ThreeDObject* object);
    bool removeObject(ThreeDObject* object);
    void pushInGraveyard(ThreeDObject* obj);
    
    bool containsObject(const ThreeDObject* obj) const;

    VkInstance getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    VkDevice getDevice() const { return device; }

private:
    int width = 800;
    int height = 600;
    
    std::string contextID;
    std::string generateContextID();

    std::list<ThreeDObject*> objects;
    std::list<ThreeDObject*> graveyard;

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    
    uint32_t graphicsQueueFamily;
    uint32_t presentQueueFamily;
};
