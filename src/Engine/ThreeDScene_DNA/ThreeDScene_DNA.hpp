#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <cstdint>
#include <string>
#include <glm/glm.hpp>
#include <vector>
#include <list>  

class ThreeDObject;
class ThreeDScene;

enum class SceneEventKind : uint8_t 
{
    InitSnapshot,
    AddObject,
    RemoveObject,
    SlotChange,
    TransformChange,
    ParentChange
};

struct SceneEvent 
{
    SceneEventKind kind{SceneEventKind::AddObject};
    std::string objectName;
    ThreeDObject* ptr{nullptr};
    uint64_t objectID{0};
    uint64_t tick{0};
    std::vector<std::string> initNames;
    std::vector<ThreeDObject*> initPtrs;
    std::vector<int> initSlots;
    int oldSlots{-1};
    int newSlots{-1};
    
    glm::mat4 oldTransform{1.0f};
    glm::mat4 newTransform{1.0f};
    uint64_t transformID{0};

    ThreeDObject* oldParent{nullptr};
    ThreeDObject* newParent{nullptr};
    uint64_t oldParentID{0};
    uint64_t newParentID{0};
    int oldSlotBeforeParent{-1};
    
};

class ThreeDScene_DNA
{
public:
    std::string uuid; 
    std::string name{"SceneDNA"};

    void ensureInit();

    bool isInitialized() const { return hasInit; }
    uint64_t initTick() const { return init_tick; }

    void trackAddObject (const std::string& name, ThreeDObject* obj);
    void trackRemoveObject(const std::string& name, ThreeDObject* obj);

    // --- hierarchy modification tracking 
    void trackSlotChange(const std::string& name, ThreeDObject* obj, int oldSlot, int newSlot);
    void trackParentChange(const std::string& name, ThreeDObject* obj, ThreeDObject* oldParent, ThreeDObject* newParent, int oldSlot = -1);
    void cancelParentChangeFromScene_DNA(uint64_t objectID);

    // --- transform tracking for non-parented objects
    void trackTransformChange(const std::string& name, ThreeDObject* obj, const glm::mat4& oldTransform, const glm::mat4& newTransform, uint64_t transformID = 0);
    bool isObjectNonParented(ThreeDObject* obj) const;
    void printTransformHistory() const;
    void syncWithMeshDNA(ThreeDObject* obj);
    void syncAllObjectsWithMeshDNA();


    const std::vector<SceneEvent>& getHistory() const { return history; }
    size_t size() const { return history.size(); }

    void finalizeBootstrap();

    bool rewindToSceneEvent(size_t index);
    void cancelLastAddObject(size_t preserveIndex = size_t(-1));
    void cancelLastRemoveObject(size_t preserveIndex = size_t(-1));
    void cancelRemoveObjectByID(uint64_t objectID);
    void cancelAddObjectByID(uint64_t objectID);
    void cancelLastSlotChange(size_t preserveIndex = size_t(-1));
    void cancelSlotChangeByID(uint64_t objectID);
    void cancelLastTransformChange(size_t preserveIndex = size_t(-1));
    
    void resurrectChildrenRecursive(ThreeDObject* parent);
    
 
    bool cancelTransformByID(uint64_t transformID);

    void setSceneRef(ThreeDScene* scene);
    ThreeDScene* getSceneRef() const { return sceneRef; }

    uint64_t generateTransformID() { return nextTransformID++; }

private:
    bool hasInit{false};
    uint64_t init_tick{0};
    uint64_t nextTick{1};
    uint64_t nextTransformID{1}; 
    std::vector<SceneEvent> history;

    bool bootstrapping{true};
    std::vector<std::string> bootstrapNames;
    std::vector<ThreeDObject*> bootstrapPtrs;
    ThreeDScene* sceneRef{nullptr};

};
