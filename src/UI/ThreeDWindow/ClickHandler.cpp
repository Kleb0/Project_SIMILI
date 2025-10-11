#include "UI/ThreeDWindow/ClickHandler.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "Engine/ThreeDScene.hpp"
#include "UI/ObjectInspectorLogic/ObjectInspector.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"

#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Camera/Camera.hpp"

#include "Engine/ErrorBox.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

ClickHandler::ClickHandler(ThreeDWindow* owner) : window(owner) {}

void ClickHandler::handle() 
{
    scene = window->getThreeDScene();
    if (!scene) {
        std::cerr << "[CLICK HANDLER] Error: No ThreeDScene available" << std::endl;
        return;
    }

    if (window->selectionLocked) 
    {
        window->selectionLocked = false;
        return;
    }

    ImVec2 mouse = ImGui::GetMousePos();
    float relativeMouseX = mouse.x - window->oglChildPos.x;
    float relativeMouseY = mouse.y - window->oglChildPos.y;
    relativeMouseY = window->oglChildSize.y - relativeMouseY;

    if (relativeMouseX >= 0 && relativeMouseX <= window->oglChildSize.x &&
        relativeMouseY >= 0 && relativeMouseY <= window->oglChildSize.y)
    {
        int windowWidth = static_cast<int>(window->oglChildSize.x);
        int windowHeight = static_cast<int>(window->oglChildSize.y);

        window->view = window->scene->getViewMatrix();
        window->proj = window->scene->getProjectionMatrix();
        
        auto& listRef = scene->getObjectsRef();
        std::vector<ThreeDObject*> objects;
        
        if (listRef.empty()) 
        {
            std::cout << "[CLICK HANDLER] No objects in scene or scene not ready" << std::endl;
            return;
        }
        
        objects.reserve(listRef.size());
        for (auto* o : listRef) 
        {
            if (o)
            {
                objects.push_back(o);
            }
        }

        if (window->currentMode == &window->normalMode)
        {
            bool preventSelection = ImGuizmo::IsOver();
            if (!preventSelection)
            {
                // showErrorBox("ClickHandler:: Test ");

                window->selector.pickUpMesh((int)relativeMouseX, (int)relativeMouseY,
                windowWidth, windowHeight, window->view, window->proj, objects);
            }

            ThreeDObject* selected = window->selector.getSelectedObject();
            bool shiftPressed = ImGui::GetIO().KeyShift;

            if (selected)
            {
                if (!shiftPressed)
                {
                    for (auto* obj : objects) obj->setSelected(false);
                    window->multipleSelectedObjects.clear();
                }

                auto it = std::find(window->multipleSelectedObjects.begin(), window->multipleSelectedObjects.end(), selected);
                
                if (it == window->multipleSelectedObjects.end())
                {
                    window->multipleSelectedObjects.push_back(selected);
                    selected->setSelected(true);
                    try 
                    {
                        glm::mat4 globalMatrix = selected->getGlobalModelMatrix();
                        glm::vec3 worldPos = glm::vec3(globalMatrix[3]);
                        std::cout << "[CLICK HANDLER] : Selected object world position: ("
                                << worldPos.x << ", "
                                << worldPos.y << ", "
                                << worldPos.z << ")" 
                        << " for object with name " << selected->getName() << std::endl;
                    } 
                    catch (const std::exception& e) 
                    {
                        std::cerr << "[CLICK HANDLER] Error getting object position: " << e.what() << std::endl;
                    }
                }
                else if (shiftPressed)
                {
                    window->multipleSelectedObjects.erase(it);
                    selected->setSelected(false);
                }

                if (window->objectInspector)
                {
                    if (window->multipleSelectedObjects.size() > 1)
                    {
                        window->objectInspector->clearInspectedObject();
                        window->objectInspector->setMultipleInspectedObjects(window->multipleSelectedObjects);
                    }
                    else
                    {
                        window->objectInspector->setInspectedObject(selected);
                    }
                }

                if (window->hierarchy) window->hierarchy->selectFromThreeDWindow();
                window->selector.clearTarget();
            }
            else if (!ImGuizmo::IsUsing() && !window->wasUsingGizmoLastFrame)
            {
                for (auto* obj : objects) obj->setSelected(false);
                window->multipleSelectedObjects.clear();
                window->selector.clearTarget();

                if (window->objectInspector)
                {
                    window->objectInspector->clearInspectedObject();
                    window->objectInspector->clearMultipleInspectedObjects();
                }

                if (window->hierarchy)
                {
                    window->hierarchy->clearMultipleSelection();
                    window->hierarchy->unselectObject(window->hierarchy->getSelectedObject());
                }

                window->wasUsingGizmoLastFrame = false;
            }
        }

        if (window->currentMode == &window->verticeMode)
        {
            if (ImGuizmo::IsUsing()) return;
            bool shiftPressed = ImGui::GetIO().KeyShift;

            Vertice* selectedVertice = window->selector.pickUpVertice(
                (int)relativeMouseX, (int)relativeMouseY,
                windowWidth, windowHeight, window->view, window->proj,
                objects, shiftPressed
            );

            if (selectedVertice)
            {
                if (shiftPressed)
                {
                    auto it = std::find(window->multipleSelectedVertices.begin(), window->multipleSelectedVertices.end(), selectedVertice);
                    if (it == window->multipleSelectedVertices.end())
                    {
                        window->multipleSelectedVertices.push_back(selectedVertice);
                        for (Vertice* v : window->multipleSelectedVertices) v->setSelected(true);
                    }
                }
                else
                {
                    for (ThreeDObject* obj : objects)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;    
                        for (Vertice* v : mesh->getVertices()) v->setSelected(false);
                    }
                    window->multipleSelectedVertices.clear();
                    selectedVertice->setSelected(true);
                    window->multipleSelectedVertices.push_back(selectedVertice);
                    window->lastSelectedVertice = selectedVertice;
                }
            }
            else
            {
                if (!shiftPressed)
                {
                    for (ThreeDObject* obj : objects)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;  
                        for (Vertice* vert : mesh->getVertices()) vert->setSelected(false);
                    }
                    window->multipleSelectedVertices.clear();
                    window->lastSelectedVertice = nullptr;
                }
            }
        }

        if (window->currentMode == &window->faceMode)
        {
            if (ImGuizmo::IsUsing()) return;
            bool shiftPressed = ImGui::GetIO().KeyShift;

            Face* selectedFace = window->selector.pickupFace(
                static_cast<int>(relativeMouseX), static_cast<int>(relativeMouseY),
                windowWidth, windowHeight, window->view, window->proj, objects, shiftPressed
            );

            if (selectedFace)
            {
                if (shiftPressed)
                {
                    auto it = std::find(window->multipleSelectedFaces.begin(), window->multipleSelectedFaces.end(), selectedFace);
                    if (it == window->multipleSelectedFaces.end())
                    {
                        window->multipleSelectedFaces.push_back(selectedFace);
                        for (Face* f : window->multipleSelectedFaces) if (f) f->setSelected(true);
                    }
                }
                else
                {
                    for (ThreeDObject* obj : objects)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;
                        for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
                    }
                    window->multipleSelectedFaces.clear();
                    selectedFace->setSelected(true);
                    window->multipleSelectedFaces.push_back(selectedFace);
                }
            }
            else
            {
                if (!shiftPressed)
                {
                    for (ThreeDObject* obj : objects)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;
                        for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
                    }
                    window->multipleSelectedFaces.clear();
                }
            }
        }

        if (window->currentMode == &window->edgeMode)
        {
            if (ImGuizmo::IsUsing()) return;
            bool shiftPressed = ImGui::GetIO().KeyShift;

            Edge* selectedEdge = window->selector.pickupEdge(
                static_cast<int>(relativeMouseX), static_cast<int>(relativeMouseY),
                windowWidth, windowHeight, window->view, window->proj, objects, shiftPressed
            );

            if (selectedEdge)
            {
                if (shiftPressed)
                {
                    auto it = std::find(window->multipleSelectedEdges.begin(), window->multipleSelectedEdges.end(), selectedEdge);
                    if (it == window->multipleSelectedEdges.end())
                    {
                        window->multipleSelectedEdges.push_back(selectedEdge);
                        for (Edge* e : window->multipleSelectedEdges) if (e) e->setSelected(true);
                        
                    }
                }
                else
                {
                    for (ThreeDObject* obj : objects)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;
                        for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
                        for (Face* f : mesh->getFaces()) if (f) f->setColor(glm::vec4(1.0f)); 
                    }
                    
                    window->multipleSelectedEdges.clear();
                    selectedEdge->setSelected(true);
                    window->multipleSelectedEdges.push_back(selectedEdge);
                    
                    // const auto& sharedFaces = selectedEdge->getSharedFaces();
                    // const glm::vec4 blueColor(0.1f, 0.3f, 1.0f, 1.0f); 
                    // for (Face* face : sharedFaces)
                    // {
                    //     if (face)
                    //     {
                    //         face->setColor(blueColor);
                    //     }
                    // }
                    
                    // std::cout << "[CLICK HANDLER] Edge selected with " << sharedFaces.size() << " shared faces colored in red" << std::endl;
                }
            }
            else
            {
                if (!shiftPressed)
                {
                    for (ThreeDObject* obj : objects)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;    
                        for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
                        for (Face* f : mesh->getFaces()) if (f) f->setColor(glm::vec4(1.0f)); 
                    }
                    window->multipleSelectedEdges.clear();
                }
            }
        }
    }
}
