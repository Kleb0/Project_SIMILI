#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <WorldObjects/Entities/ThreeDObject.hpp>
#include <iostream>
#include <list>
#include <string>

class VKContext;
class Camera;
class Vertice;
class Edge;
class Face;
class SceneObjectContainer;

struct VKGraveyardEntry 
{
    ThreeDObject* object;
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;
    std::vector<Face*> faces;
};

class VKScene
{
public:
    VKScene();
    ~VKScene();

    void setVKContext(VKContext* ctx) { vkctx = ctx; }
    VKContext* getVKContext() const { return vkctx; }

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjMatrix() const;

    void initialize();
    void resize(int w, int h);
    void render(int width, int height);

    std::list<ThreeDObject*>& getObjectsRef();
    void addObject(ThreeDObject* object);
    bool removeObject(ThreeDObject* object);

    void setSceneObjectContainer(SceneObjectContainer* container);

    void pushInGraveyard(ThreeDObject* obj);
    const std::list<ThreeDObject*>& getGraveyard() const { return graveyard; }
    const std::vector<VKGraveyardEntry>& getMeshGraveyard() const { return meshGraveyard; }

    void setActiveCamera(Camera* cam);
    Camera* getActiveCamera() const { return activeCamera; }
    void setCameraToUse(Camera* cam);

    bool containsObject(const ThreeDObject* obj) const;

    std::string getSceneID() const { return sceneID; }

    void bindSceneViewToWorkSpaceDimensions(int logicalX, int logicalY, int logicalWidth, int logicalHeight,
        int drawableWidth, int drawableHeight, int logicalWindowWidth, int logicalWindowHeight);

    void renderRedScreenOnWorkSpaceDimensionsFirst(VkCommandBuffer commandBuffer, int mouseX, int mouseY);

private:
    glm::mat4 lastViewProj{1.0f};
    std::list<ThreeDObject*> objects;
    std::list<ThreeDObject*> graveyard;
    std::vector<VKGraveyardEntry> meshGraveyard;

    Camera* activeCamera{nullptr};
    VKContext* vkctx{nullptr};
    SceneObjectContainer* objectContainer_{nullptr};

    std::string sceneID;
    std::string generateSceneID();

    int scene_viewport_x_ = 0;
    int scene_viewport_y_ = 0;
    int scene_viewport_width_ = 0;
    int scene_viewport_height_ = 0;
    bool scene_viewport_bound_ = false;

    int workspace_logical_x_ = 0;
    int workspace_logical_y_ = 0;
    int workspace_logical_width_ = 0;
    int workspace_logical_height_ = 0;

    bool hover_active_ = false;
    float hover_r_ = 0.22f;
    float hover_g_ = 0.28f;
    float hover_b_ = 0.35f;

    VkPipeline graphicsPipeline;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    VkBuffer gridVertexBuffer;
    VkDeviceMemory gridVertexBufferMemory;
    uint32_t gridVertexCount;
};
