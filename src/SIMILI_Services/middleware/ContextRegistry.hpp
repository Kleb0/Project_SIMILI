#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstdint>
#include <optional>
#include <vector>

// Forward declarations
class OpenGLContext;
class ThreeDScene;

namespace SIMILI {
namespace Server {


struct OpenGLContextComponent 
{
    OpenGLContext* context = nullptr;
    std::string contextID;
    
    OpenGLContextComponent() = default;
    OpenGLContextComponent(OpenGLContext* ctx, const std::string& id) 
        : context(ctx), contextID(id) {}
};

struct SceneComponent 
{
    ThreeDScene* scene = nullptr;
    std::string sceneID;
    
    SceneComponent() = default;
    SceneComponent(ThreeDScene* s, const std::string& id) 
        : scene(s), sceneID(id) {}
};

struct MetadataComponent 
{
    std::string processName;
    std::string creationTime;
    
    MetadataComponent() = default;
    MetadataComponent(const std::string& name, const std::string& time)
        : processName(name), creationTime(time) {}
};

using EntityID = uint64_t;


class ContextRegistry 
{
public:
    ContextRegistry() : nextEntityID_(1) {}
    ~ContextRegistry() = default;
    
    // ===== Entity Management =====
    

    EntityID createEntity();
    

    void destroyEntity(EntityID entity);
    

    bool entityExists(EntityID entity) const;
    void addOpenGLContextComponent(EntityID entity, OpenGLContext* context, const std::string& contextID);
    
    void addSceneComponent(EntityID entity, ThreeDScene* scene, const std::string& sceneID);

    void addMetadataComponent(EntityID entity, const std::string& processName, const std::string& creationTime);
    

    std::optional<OpenGLContextComponent> getOpenGLContextComponent(EntityID entity) const;

    std::optional<SceneComponent> getSceneComponent(EntityID entity) const;
    
    std::optional<MetadataComponent> getMetadataComponent(EntityID entity) const;

    void removeOpenGLContextComponent(EntityID entity);
    
    void removeSceneComponent(EntityID entity);
    
    void removeMetadataComponent(EntityID entity);
    
    std::optional<EntityID> findEntityByContextID(const std::string& contextID) const;
    
    std::optional<EntityID> findEntityBySceneID(const std::string& sceneID) const;
    

    std::optional<EntityID> findEntityByProcessName(const std::string& processName) const;

    std::vector<EntityID> getEntitiesWithOpenGLContext() const;
    
    std::vector<EntityID> getEntitiesWithScene() const;

private:
    mutable std::mutex mutex_;
    
    // Entity counter
    EntityID nextEntityID_;
    
    // Component storage (sparse sets)
    std::unordered_map<EntityID, OpenGLContextComponent> openGLContextComponents_;
    std::unordered_map<EntityID, SceneComponent> sceneComponents_;
    std::unordered_map<EntityID, MetadataComponent> metadataComponents_;
    

    std::unordered_map<EntityID, bool> entities_;
};

} 
} 
