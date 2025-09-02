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
    std::cout << "[ThreeDScene_DNA] " << k << " object='" << name
              << "' tick=" << history.back().tick << std::endl;
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

    // std::cout << "[ThreeDScene_DNA] INIT snapshot recorded with "
    //           << history.front().initNames.size() << " object(s) at tick=0\n";
}