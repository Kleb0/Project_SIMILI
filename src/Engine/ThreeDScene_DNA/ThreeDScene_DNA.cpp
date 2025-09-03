#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
#include "Engine/ThreeDScene.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include <algorithm>
#include <iostream>


static inline void erasePtr(std::list<ThreeDObject*>& L, ThreeDObject* p) 
{
    L.remove(p); 
}


void ThreeDScene_DNA::ensureInit()
{
    if (hasInit) return;
    hasInit = true;
    init_tick = 0;

    std::cout << "[ThreeDScene_DNA] Initialized once (tick=" << init_tick << ")" << " | name=" << name << std::endl;
}

void ThreeDScene_DNA::track(SceneEventKind kind, const std::string& name, ThreeDObject* obj)
{
    SceneEvent ev;
    ev.kind = kind;
    ev.objectName = name;
    ev.ptr = obj;
    ev.tick = nextTick++;
    history.push_back(std::move(ev));

    const char* k = (kind == SceneEventKind::AddObject ? "ADD" :
    kind == SceneEventKind::RemoveObject ? "REMOVE" : "INIT");

}

void ThreeDScene_DNA::trackAddObject(const std::string& name, ThreeDObject* obj)
{
    if (bootstrapping) 
    {
        bootstrapNames.push_back(name);
        bootstrapPtrs.push_back(obj);
        return;
    }
    track(SceneEventKind::AddObject, name, obj);
}

void ThreeDScene_DNA::trackRemoveObject(const std::string& name, ThreeDObject* obj)
{
    track(SceneEventKind::RemoveObject, name, obj);
}

void ThreeDScene_DNA::finalizeBootstrap()
{
    ensureInit();
    if (!bootstrapping) return;

    SceneEvent snap;
    snap.kind = SceneEventKind::InitSnapshot;
    snap.tick = 0;
    snap.initNames = bootstrapNames;
    snap.initPtrs = bootstrapPtrs;
    history.insert(history.begin(), std::move(snap));

    bootstrapNames.clear();
    bootstrapPtrs.clear();
    bootstrapping = false;
    nextTick = 1;
}

bool ThreeDScene_DNA::rewindToSceneEvent(size_t index)
{
    if (index >= history.size())
        return false;

    auto it = history.begin() + index + 1;
    if (it == history.end())
        return false;

    history.erase(it, history.end());
    return true;
}


void ThreeDScene_DNA::cancelLastAddObject(size_t preserveIndex)
{
    if (history.empty()) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::next(it).base();
        size_t idx = std::distance(history.begin(), baseIt);

        if (it->kind == SceneEventKind::AddObject && idx != preserveIndex)
        {
            auto name = it->objectName;
            history.erase(baseIt);
            return;
        }
    }
}

