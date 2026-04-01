#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <list>

struct SDL_Window;

class ThreeDObject;
class ThreeDScreen;
class Camera;

class VKContext
{
public:
    VKContext();
    ~VKContext();

    void initialize(SDL_Window* window = nullptr);
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
    VkQueue getGraphicsQueue() const { return graphicsQueue; }
    VkQueue getPresentQueue() const { return presentQueue; }
    uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamily; }
    uint32_t getPresentQueueFamily() const { return presentQueueFamily; }
    VkSurfaceKHR getSurface() const { return surface_; }
    void setThreeDScreen(ThreeDScreen* screen) { three_d_screen_ = screen; }
    ThreeDScreen* getThreeDScreen() const { return three_d_screen_; }

    void setCamera(Camera* cam);
    Camera* getCamera() const { return camera_; }
    void ProjectOnThreeDScreen();

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
    VkSurfaceKHR surface_;
    VkDebugUtilsMessengerEXT debugMessenger_;
    
    uint32_t graphicsQueueFamily;
    uint32_t presentQueueFamily;
    ThreeDScreen* three_d_screen_ = nullptr;
    Camera* camera_ = nullptr;
};
