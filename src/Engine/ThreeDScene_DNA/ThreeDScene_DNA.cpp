#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
#include "Engine/ThreeDScene.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include <algorithm>
#include <iostream>
#include "Engine/ErrorBox.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"


static inline void erasePtr(std::list<ThreeDObject*>& L, ThreeDObject* p) 
{
    L.remove(p); 
}

void ThreeDScene_DNA::setSceneRef(ThreeDScene* scene) 
{
    sceneRef = scene;
}


void ThreeDScene_DNA::ensureInit()
{
    if (hasInit) return;
    hasInit = true;
    init_tick = 0;

    std::cout << "[ThreeDScene_DNA] Initialized once (tick=" << init_tick << ")" << " | name=" << name << std::endl;
}

void ThreeDScene_DNA::trackAddObject(const std::string& name, ThreeDObject* obj)
{
    if (bootstrapping) 
    {
        bootstrapNames.push_back(name);
        bootstrapPtrs.push_back(obj);
        return;
    }


    SceneEvent ev;
    ev.kind = SceneEventKind::AddObject;
    ev.objectName = name;
    ev.ptr = obj;
    ev.objectID = obj->getID();
    ev.tick = nextTick++;
    history.push_back(std::move(ev));

    std::cout << "[SceneDNA] Tracked ADD | name: " << name
    << " | id: " << ev.objectID
    << " | tick: " << ev.tick << std::endl;
}

void ThreeDScene_DNA::trackRemoveObject(const std::string& name, ThreeDObject* obj)
{
    SceneEvent ev;
    ev.kind = SceneEventKind::RemoveObject;
    ev.objectName = name;
    ev.ptr = obj;
    ev.objectID = obj ? obj->getID() : 0;
    ev.tick = nextTick++;
    history.push_back(std::move(ev));

    std::cout << "[SceneDNA] Tracked REMOVE | name: " << name
              << " | tick: " << ev.tick << std::endl;
}

// -------- hierarchy modification tracking -------- //

void ThreeDScene_DNA::trackSlotChange(const std::string& name, ThreeDObject* obj, int oldSlot, int newSlot)
{
    SceneEvent ev;
    ev.kind = SceneEventKind::SlotChange;
    ev.objectName = name;
    ev.ptr = obj;
    ev.objectID = obj ? obj->getID() : 0;
    ev.oldSlots = oldSlot;
    ev.newSlots = newSlot;
    ev.tick = nextTick++;
    history.push_back(std::move(ev));
    std::cout << "[SceneDNA] Tracked SLOT CHANGE | name: " << name
    << " | id: " << (obj ? obj->getID() : 0)
    << " | from: " << oldSlot << " to: " << newSlot
    << " | tick: " << (nextTick - 1) << std::endl;
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

    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());
        size_t idx = std::distance(history.begin(), baseIt);

        if (it->kind == SceneEventKind::AddObject && idx == preserveIndex)
        {
            uint64_t targetID = it->objectID;

            for (ThreeDObject* obj : sceneRef->getObjectsRef())
            {
                if (obj && obj->getID() == targetID)
                {
                    sceneRef->removeObjectFromSceneDNA(targetID);
                    break;
                }
            }
        
            history.erase(baseIt);
            return;
        }
    }
}

void ThreeDScene_DNA::cancelLastRemoveObject(size_t preserveIndex)
{
    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());
        size_t idx = static_cast<size_t>(std::distance(history.begin(), baseIt));

        if (it->kind == SceneEventKind::RemoveObject && idx == preserveIndex)
        {
            const uint64_t targetID = it->objectID;
            ThreeDObject* resurrect = nullptr;

            const auto& gyConst = sceneRef->getGraveyard();
            auto& gy = const_cast<std::list<ThreeDObject*>&>(gyConst);

            for (auto git = gy.begin(); git != gy.end(); ++git)
            {
                ThreeDObject* o = *git;
                if (o && o->getID() == targetID)
                {
                    resurrect = o;
                    gy.erase(git);
                    break;
                }
            }

            auto& objects = sceneRef->getObjectsRef();
            if (!resurrect)
            {
                for (auto* o : objects)
                {
                    if (o && o->getID() == targetID) { resurrect = o; break; }
                }
            }

            if (resurrect)
            {
                resurrect->setSelected(false);
                glm::mat4 G = resurrect->getGlobalModelMatrix();
                resurrect->removeParent();
                resurrect->setModelMatrix(G);

                if (std::find(objects.begin(), objects.end(), resurrect) == objects.end())
                    objects.push_back(resurrect);

                if (auto* mesh = dynamic_cast<Mesh*>(resurrect))
                {
                    if (!mesh->getMeshDNA())
                    {
                        auto* dna = new MeshDNA();
                        dna->name = resurrect->getName();
                        mesh->setMeshDNA(dna, true);
                    }
                    mesh->getMeshDNA()->ensureInit(mesh->getModelMatrix());
                }
            }

            std::cout << "[ThreeDScene_DNA] Resurrected object from graveyard: " << resurrect->getName() << " (ID=" << targetID << ")" << std::endl;
            sceneRef->getHierarchyInspector()->redrawSlotsList();
            // sceneRef->getThreeDWindow()->addToObjectList(resurrect);
            history.erase(baseIt);
            return;
        }
    }
}
