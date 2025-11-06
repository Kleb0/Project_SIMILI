#include "ContextRegistry.hpp"
#include <algorithm>
#include <iostream>

namespace SIMILI {
namespace Server {

// ===== Entity Management =====

EntityID ContextRegistry::createEntity() 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    EntityID id = nextEntityID_++;
    entities_[id] = true;
    
    std::cout << "[ContextRegistry] Created entity " << id << std::endl;
    return id;
}

void ContextRegistry::destroyEntity(EntityID entity) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (entities_.find(entity) == entities_.end()) {
        std::cerr << "[ContextRegistry] Entity " << entity << " does not exist" << std::endl;
        return;
    }
    
    // Remove all components
    openGLContextComponents_.erase(entity);
    sceneComponents_.erase(entity);
    metadataComponents_.erase(entity);
    
    // Remove entity
    entities_.erase(entity);
    
    std::cout << "[ContextRegistry] Destroyed entity " << entity << std::endl;
}

bool ContextRegistry::entityExists(EntityID entity) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entities_.find(entity) != entities_.end();
}

// ===== Component Management =====

void ContextRegistry::addOpenGLContextComponent(EntityID entity, OpenGLContext* context, const std::string& contextID) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (entities_.find(entity) == entities_.end()) {
        std::cerr << "[ContextRegistry] Cannot add component: entity " << entity << " does not exist" << std::endl;
        return;
    }
    
    openGLContextComponents_[entity] = OpenGLContextComponent(context, contextID);
    std::cout << "[ContextRegistry] Added OpenGLContext component to entity " << entity 
              << " (ID: " << contextID << ")" << std::endl;
}

void ContextRegistry::addSceneComponent(EntityID entity, ThreeDScene* scene, const std::string& sceneID) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (entities_.find(entity) == entities_.end()) {
        std::cerr << "[ContextRegistry] Cannot add component: entity " << entity << " does not exist" << std::endl;
        return;
    }
    
    sceneComponents_[entity] = SceneComponent(scene, sceneID);
    std::cout << "[ContextRegistry] Added Scene component to entity " << entity 
              << " (ID: " << sceneID << ")" << std::endl;
}

void ContextRegistry::addMetadataComponent(EntityID entity, const std::string& processName, const std::string& creationTime) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (entities_.find(entity) == entities_.end()) {
        std::cerr << "[ContextRegistry] Cannot add component: entity " << entity << " does not exist" << std::endl;
        return;
    }
    
    metadataComponents_[entity] = MetadataComponent(processName, creationTime);
    std::cout << "[ContextRegistry] Added Metadata component to entity " << entity 
              << " (Process: " << processName << ")" << std::endl;
}

std::optional<OpenGLContextComponent> ContextRegistry::getOpenGLContextComponent(EntityID entity) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = openGLContextComponents_.find(entity);
    if (it != openGLContextComponents_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<SceneComponent> ContextRegistry::getSceneComponent(EntityID entity) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sceneComponents_.find(entity);
    if (it != sceneComponents_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<MetadataComponent> ContextRegistry::getMetadataComponent(EntityID entity) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = metadataComponents_.find(entity);
    if (it != metadataComponents_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ContextRegistry::removeOpenGLContextComponent(EntityID entity) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    openGLContextComponents_.erase(entity);
}

void ContextRegistry::removeSceneComponent(EntityID entity) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    sceneComponents_.erase(entity);
}

void ContextRegistry::removeMetadataComponent(EntityID entity) 
{
    std::lock_guard<std::mutex> lock(mutex_);
    metadataComponents_.erase(entity);
}

// ===== Query System =====

std::optional<EntityID> ContextRegistry::findEntityByContextID(const std::string& contextID) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [entity, component] : openGLContextComponents_) {
        if (component.contextID == contextID) {
            return entity;
        }
    }
    return std::nullopt;
}

std::optional<EntityID> ContextRegistry::findEntityBySceneID(const std::string& sceneID) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [entity, component] : sceneComponents_) {
        if (component.sceneID == sceneID) {
            return entity;
        }
    }
    return std::nullopt;
}

std::optional<EntityID> ContextRegistry::findEntityByProcessName(const std::string& processName) const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& [entity, component] : metadataComponents_) {
        if (component.processName == processName) {
            return entity;
        }
    }
    return std::nullopt;
}

std::vector<EntityID> ContextRegistry::getEntitiesWithOpenGLContext() const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<EntityID> result;
    for (const auto& [entity, _] : openGLContextComponents_) {
        result.push_back(entity);
    }
    return result;
}

std::vector<EntityID> ContextRegistry::getEntitiesWithScene() const 
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<EntityID> result;
    for (const auto& [entity, _] : sceneComponents_) {
        result.push_back(entity);
    }
    return result;
}

} // namespace Server
} // namespace SIMILI
