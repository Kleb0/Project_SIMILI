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
    SlotChange
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


    const std::vector<SceneEvent>& getHistory() const { return history; }
    size_t size() const { return history.size(); }

    void finalizeBootstrap();

    bool rewindToSceneEvent(size_t index);
    void cancelLastAddObject(size_t preserveIndex = size_t(-1));
    void cancelLastRemoveObject(size_t preserveIndex = size_t(-1));
    void cancelLastSlotChange(size_t preserveIndex = size_t(-1));

    void setSceneRef(ThreeDScene* scene);
    ThreeDScene* getSceneRef() const { return sceneRef; }


private:
    bool hasInit{false};
    uint64_t init_tick{0};
    uint64_t nextTick{1};
    std::vector<SceneEvent> history;

    bool bootstrapping{true};
    std::vector<std::string> bootstrapNames;
    std::vector<ThreeDObject*> bootstrapPtrs;
    ThreeDScene* sceneRef{nullptr};

};
