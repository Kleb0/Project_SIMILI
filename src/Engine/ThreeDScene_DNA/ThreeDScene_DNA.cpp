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
    if (bootstrapping) return;
    
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

void ThreeDScene_DNA::trackParentChange(const std::string& name, ThreeDObject* obj, ThreeDObject* oldParent, ThreeDObject* newParent, int oldSlot)
{
    if (bootstrapping) return;
    
    SceneEvent ev;
    ev.kind = SceneEventKind::ParentChange;
    ev.objectName = name;
    ev.ptr = obj;
    ev.objectID = obj ? obj->getID() : 0;
    ev.oldParent = oldParent;
    ev.newParent = newParent;
    ev.oldParentID = oldParent ? oldParent->getID() : 0;
    ev.newParentID = newParent ? newParent->getID() : 0;
    ev.oldSlotBeforeParent = oldSlot;
    ev.tick = nextTick++;
    history.push_back(std::move(ev));
    
    std::cout << "[SceneDNA] Tracked PARENT CHANGE | name: " << name
    << " | id: " << (obj ? obj->getID() : 0)
    << " | from: " << (oldParent ? oldParent->getName() : "null") 
    << " to: " << (newParent ? newParent->getName() : "null")
    << " | oldSlot: " << oldSlot
    << " | tick: " << (nextTick - 1) << std::endl;
}

void ThreeDScene_DNA::trackTransformChange(const std::string& name, ThreeDObject* obj, const glm::mat4& oldTransform, const glm::mat4& newTransform, uint64_t transformID)
{
    if (!obj || obj->getParent()) return;

    SceneEvent ev;
    ev.kind = SceneEventKind::TransformChange;
    ev.objectName = name;
    ev.ptr = obj;
    ev.objectID = obj->getID();
    ev.oldTransform = oldTransform;
    ev.newTransform = newTransform;
    ev.tick = nextTick++;
    ev.transformID = (transformID > 0) ? transformID : generateTransformID(); 
    history.push_back(std::move(ev));

}

bool ThreeDScene_DNA::isObjectNonParented(ThreeDObject* obj) const
{
    return obj && !obj->getParent();
}

void ThreeDScene_DNA::printTransformHistory() const
{
    std::cout << "\n=== Transform History ===" << std::endl;
    for (const auto& event : history)
    {
        if (event.kind == SceneEventKind::TransformChange)
        {
            std::cout << "Transform Change | Object: " << event.objectName 
                      << " | ID: " << event.objectID 
                      << " | Tick: " << event.tick << std::endl;
        }
    }
    std::cout << "=== End Transform History ===\n" << std::endl;
}

void ThreeDScene_DNA::syncWithMeshDNA(ThreeDObject* obj)
{
    if (!obj || obj->getParent()) return;
    
    auto* mesh = dynamic_cast<Mesh*>(obj);
    if (!mesh || !mesh->getMeshDNA()) return;
    
    const auto& meshHistory = mesh->getMeshDNA()->getHistory();
    if (meshHistory.empty()) return;
    
    for (const auto& meshEvent : meshHistory)
    {
        if (meshEvent.isComponentEdit) continue;
        
        glm::mat4 oldTransform = glm::inverse(meshEvent.delta) * obj->getModelMatrix();
        glm::mat4 newTransform = obj->getModelMatrix();
        
        trackTransformChange(obj->getName(), obj, oldTransform, newTransform);
    }
}

void ThreeDScene_DNA::syncAllObjectsWithMeshDNA()
{
    if (!sceneRef) return;
    
    for (ThreeDObject* obj : sceneRef->getObjectsRef())
    {
        if (obj && !obj->getParent())
        {
            syncWithMeshDNA(obj);
        }
    }
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

    snap.initSlots.reserve(bootstrapPtrs.size());
    for (ThreeDObject* obj : bootstrapPtrs)
    {
        if (obj)
            snap.initSlots.push_back(obj->getSlot());
        else
            snap.initSlots.push_back(-1); 
    }
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


void ThreeDScene_DNA::resurrectChildrenRecursive(ThreeDObject* parent)
{
    if (!parent || !sceneRef) return;
    
    auto& graveyard = const_cast<std::list<ThreeDObject*>&>(sceneRef->getGraveyard());
    auto& meshGraveyard = const_cast<std::vector<GraveyardEntry>&>(sceneRef->getMeshGraveyard());
    auto& objects = sceneRef->getObjectsRef();
    
    std::vector<ThreeDObject*> childrenToResurrect;
    
    for (auto git = graveyard.begin(); git != graveyard.end();)
    {
        ThreeDObject* child = *git;
        if (child && child->getParent() == parent)
        {
            childrenToResurrect.push_back(child);
            git = graveyard.erase(git);
        }
        else
        {
            ++git;
        }
    }
    
    for (auto* child : childrenToResurrect)
    {
        child->setSelected(false);
        
        if (std::find(objects.begin(), objects.end(), child) == objects.end())
            objects.push_back(child);
        
        if (auto* mesh = dynamic_cast<Mesh*>(child))
        {
            if (!mesh->getMeshDNA())
            {
                auto* dna = new MeshDNA();
                dna->name = child->getName();
                mesh->setMeshDNA(dna, true);
            }
            mesh->getMeshDNA()->ensureInit(mesh->getModelMatrix());
        }
        
        std::cout << "[ThreeDScene_DNA] Resurrected child object: " << child->getName() << " (ID=" << child->getID() << ")" << std::endl;
        
        resurrectChildrenRecursive(child);
    }
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
                
                resurrectChildrenRecursive(resurrect);
            }

            // std::cout << "[ThreeDScene_DNA] Resurrected object from graveyard: " << resurrect->getName() << " (ID=" << targetID << ")" << std::endl;
            sceneRef->getHierarchyInspector()->redrawSlotsList();
            history.erase(baseIt);
            return;
        }
    }
}

void ThreeDScene_DNA::cancelRemoveObjectByID(uint64_t objectID)
{
    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());

        if (it->kind == SceneEventKind::RemoveObject && it->objectID == objectID)
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
                
                if (resurrect->getParent())
                {
                    resurrect->getParent()->addChild(resurrect);
                }
                
                resurrectChildrenRecursive(resurrect);
            }

            std::cout << "[ThreeDScene_DNA] Resurrected object from graveyard by ID: " << resurrect->getName() << " (ID=" << targetID << ")" << std::endl;
            sceneRef->getHierarchyInspector()->redrawSlotsList();
            history.erase(baseIt);
            return;
        }
    }
}

void ThreeDScene_DNA::cancelAddObjectByID(uint64_t objectID)
{
    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());

        if (it->kind == SceneEventKind::AddObject && it->objectID == objectID)
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

void ThreeDScene_DNA::cancelSlotChangeByID(uint64_t objectID)
{
    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());

        if (it->kind == SceneEventKind::SlotChange && it->objectID == objectID)
        {
            ThreeDObject* obj = it->ptr;
            if (!obj) break;

            auto& changes = const_cast<std::vector<int>&>(obj->getChangedSlots());
            if (changes.empty()) break;

            int lastOldSlot = changes.back();
            changes.pop_back(); 
            int currentSlot = obj->getSlot();

            obj->setSlot(lastOldSlot);

            auto* hi = sceneRef->getHierarchyInspector();
            if (hi && lastOldSlot >= 0 && static_cast<size_t>(lastOldSlot) < hi->mergedHierarchyList.size())
            {
                hi->mergedHierarchyList[lastOldSlot] = obj;
                if (currentSlot >= 0 && static_cast<size_t>(currentSlot) < hi->mergedHierarchyList.size())
                    hi->mergedHierarchyList[currentSlot] = hi->emptySlotPlaceholders[currentSlot].get();
            }

            history.erase(baseIt);
            sceneRef->getHierarchyInspector()->redrawSlotsList();
            return;
        }
    }
}

void ThreeDScene_DNA::cancelLastSlotChange(size_t preserveIndex)
{
    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());
        size_t idx = static_cast<size_t>(std::distance(history.begin(), baseIt));

        if (it->kind == SceneEventKind::SlotChange && idx == preserveIndex)
        {
            ThreeDObject* obj = it->ptr;
            if (!obj) break;

            auto& changes = const_cast<std::vector<int>&>(obj->getChangedSlots());
            if (changes.empty()) break;

            int lastOldSlot = changes.back();
            changes.pop_back(); 
            int currentSlot = obj->getSlot();

            obj->setSlot(lastOldSlot);

            auto* hi = sceneRef->getHierarchyInspector();
            if (hi && lastOldSlot >= 0 && static_cast<size_t>(lastOldSlot) < hi->mergedHierarchyList.size())
            {
                hi->mergedHierarchyList[lastOldSlot] = obj;
                if (currentSlot >= 0 && static_cast<size_t>(currentSlot) < hi->mergedHierarchyList.size())
                    hi->mergedHierarchyList[currentSlot] = hi->emptySlotPlaceholders[currentSlot].get();
            }

            // std::cout << "[ThreeDScene_DNA] Cancelled slot change for object: "
            //           << obj->getName() << " | from: " << currentSlot
            //           << " back to: " << lastOldSlot << std::endl;

            history.erase(baseIt);
            sceneRef->getHierarchyInspector()->redrawSlotsList();
            return;
        }
    }
}

void ThreeDScene_DNA::cancelLastTransformChange(size_t preserveIndex)
{
    if (history.empty() || !sceneRef) return;

    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());
        size_t idx = static_cast<size_t>(std::distance(history.begin(), baseIt));

        if (it->kind == SceneEventKind::TransformChange && idx == preserveIndex)
        {
            ThreeDObject* obj = it->ptr;
            if (!obj || obj->getParent()) break;

            if (auto* mesh = dynamic_cast<Mesh*>(obj))
            {
                if (auto* meshDNA = mesh->getMeshDNA())
                {
                    meshDNA->cancelTransformByID(it->transformID, mesh);
                }
            }

            obj->setModelMatrix(it->oldTransform);
            history.erase(baseIt);
            return;
        }
    }
}

bool ThreeDScene_DNA::cancelTransformByID(uint64_t transformID)
{
    if (transformID == 0) return false;
    
    auto it = std::find_if(history.begin(), history.end(),
        [transformID](const SceneEvent& ev) 
        {
            return ev.kind == SceneEventKind::TransformChange && ev.transformID == transformID;
        });
    
    if (it == history.end()) return false;
    
    ThreeDObject* obj = it->ptr;
    if (!obj || obj->getParent()) return false;
    
    if (auto* mesh = dynamic_cast<Mesh*>(obj))
    {
        if (auto* meshDNA = mesh->getMeshDNA())
        {
            meshDNA->cancelTransformByID(transformID, mesh);
        }
    }
    
    obj->setModelMatrix(it->oldTransform);
    
    std::string objectName = it->objectName;
    
    history.erase(it);
    
    
    return true;
}

void ThreeDScene_DNA::cancelParentChangeFromScene_DNA(uint64_t objectID)
{
    if (history.empty() || !sceneRef) return;
    
    for (auto it = history.rbegin(); it != history.rend(); ++it)
    {
        auto baseIt = std::prev(it.base());
        if (it->kind == SceneEventKind::ParentChange && it->objectID == objectID)
        {
            ThreeDObject* obj = it->ptr;
            ThreeDObject* oldParent = it->oldParent;
            ThreeDObject* newParent = it->newParent;
            int oldSlot = it->oldSlotBeforeParent;
            
            if (!obj) 
            {
                std::cerr << "[SceneDNA] ERROR: Object pointer is null for ParentChange cancellation." << std::endl;
                history.erase(baseIt);
                return;
            }
            
            if (newParent)
            {
                newParent->removeChild(obj);
            }
            
            if (oldParent)
            {
                oldParent->addChild(obj);
                obj->setParent(oldParent);
                obj->isParented = true;
            }
            else
            {
                obj->removeParent();
                obj->isParented = false;
            }
            
            if (oldSlot >= 0 && sceneRef->getHierarchyInspector())
            {
                auto* hierarchyInspector = sceneRef->getHierarchyInspector();
                auto& mergedList = hierarchyInspector->getMergedHierarchyList();
                auto& emptyPlaceholders = hierarchyInspector->getEmptySlotPlaceholders();
                
                if (oldSlot < static_cast<int>(mergedList.size()))
                {
                    int currentSlot = obj->getSlot();
                    if (currentSlot >= 0 && currentSlot < static_cast<int>(mergedList.size()))
                    {
                        mergedList[currentSlot] = emptyPlaceholders[currentSlot].get();
                    }
                    
                    obj->setSlot(oldSlot);
                    mergedList[oldSlot] = obj;                   

                }
            }
            

            
            if (sceneRef->getHierarchyInspector())
            {
                sceneRef->getHierarchyInspector()->redrawSlotsList();
            }
            
            history.erase(baseIt);
            return;
        }
    }
    
}
