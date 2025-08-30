#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>


enum class SceneEventKind : uint8_t {
    AddObject,
    RemoveObject
};

struct SceneEvent {
    SceneEventKind kind{SceneEventKind::AddObject};
    std::string objectName;
    uint64_t tick{0};
};

class ThreeDScene_DNA
{
public:
    std::string uuid; 
    std::string name{"SceneDNA"};

    void ensureInit();

    bool isInitialized() const { return hasInit; }
    uint64_t initTick() const { return init_tick; }

    void trackAddObject(const std::string& name);
    void trackRemoveObject(const std::string& name);

    const std::vector<SceneEvent>& getHistory() const { return history; }
    size_t size() const { return history.size(); }


private:
    bool hasInit{false};
    uint64_t init_tick{0};

    uint64_t nextTick{1};
    std::vector<SceneEvent> history;

    void track(SceneEventKind kind, const std::string& name);
};
