#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
#include <iostream>
#include <iomanip>

void ThreeDScene_DNA::ensureInit()
{
    if (hasInit) return;
    hasInit = true;
    init_tick = 1;

    std::cout << "[ThreeDScene_DNA] Initialized once (tick=" << init_tick << ")" << " | name=" << name << std::endl;
}

void ThreeDScene_DNA::track(SceneEventKind kind, const std::string& name)
{
    SceneEvent ev;
    ev.kind = kind;
    ev.objectName = name;
    ev.tick = nextTick++;
    history.push_back(std::move(ev));

    const char* k = (kind == SceneEventKind::AddObject ? "ADD" : "REMOVE");
    std::cout << "[ThreeDScene_DNA] " << k << " object='" << name
    << "' tick=" << (history.back().tick) << std::endl;
}

void ThreeDScene_DNA::trackAddObject(const std::string& name) { track(SceneEventKind::AddObject, name); }
void ThreeDScene_DNA::trackRemoveObject(const std::string& name) { track(SceneEventKind::RemoveObject, name); }
