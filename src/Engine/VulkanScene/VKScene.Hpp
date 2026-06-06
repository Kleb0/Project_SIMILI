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
class VulkanPipeline;

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

    void setVulkanPipelines(VulkanPipeline* pipelines, VkRenderPass renderPass);
    void drawThreeDScene(VkCommandBuffer commandBuffer, int vpX, int vpY, int vpW, int vpH,
        const glm::mat4& view, const glm::mat4& proj);

    std::list<ThreeDObject*> selectedObjects;
    void setSelectedObjects(const std::list<ThreeDObject*>& objs);
    const std::list<ThreeDObject*>& getSelectedObjects() const { return selectedObjects; }

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

    VulkanPipeline* vulkan_pipelines_ = nullptr;
    VkRenderPass scene_render_pass_ = VK_NULL_HANDLE;
    VkPipeline grid_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout grid_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline background_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout background_pipeline_layout_ = VK_NULL_HANDLE;
    bool scene_pipeline_initialized_ = false;
    VkBuffer background_vertex_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory background_vertex_memory_ = VK_NULL_HANDLE;

    //list of Selected ThreeDObjects for rendering selection highlights
  

    VkBuffer gridVertexBuffer;
    VkDeviceMemory gridVertexBufferMemory;
    uint32_t gridVertexCount;

    // Mesh rendering pipelines (faces=TRIANGLE_LIST, edges=LINE_LIST, vertices=POINT_LIST)
    VkPipeline mesh_face_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout mesh_face_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline mesh_edge_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout mesh_edge_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline mesh_vertex_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout mesh_vertex_pipeline_layout_ = VK_NULL_HANDLE;

    // Selected mesh element rendering pipelines
    VkPipeline mesh_selected_face_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout mesh_selected_face_pipeline_layout_ = VK_NULL_HANDLE;
    VkPipeline mesh_selected_edge_pipeline_handle_ = VK_NULL_HANDLE;
    VkPipelineLayout mesh_selected_edge_pipeline_layout_ = VK_NULL_HANDLE;

    // Dynamic host-visible buffer for per-frame mesh geometry upload
    VkBuffer mesh_dynamic_buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory mesh_dynamic_memory_ = VK_NULL_HANDLE;
    VkDeviceSize mesh_dynamic_capacity_ = 0;

    bool initializeScenePipelines();
    bool createGridVertexBuffer();
    bool createBackgroundVertexBuffer();
    bool createVulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memProps, VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    bool ensureMeshBuffer(VkDeviceSize neededSize);
    void drawMeshObjects(VkCommandBuffer commandBuffer, const glm::mat4& view, const glm::mat4& proj);
};
