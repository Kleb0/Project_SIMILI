#include "UI/ThreeDWindow/ClickHandler.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "UI/ObjectInspectorLogic/ObjectInspector.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"

#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Camera/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

ClickHandler::ClickHandler(ThreeDWindow* owner) : w(owner) {}

void ClickHandler::handle() {
    if (w->selectionLocked) {
        w->selectionLocked = false;
        return;
    }

    ImVec2 mouse = ImGui::GetMousePos();
    float relativeMouseX = mouse.x - w->oglChildPos.x;
    float relativeMouseY = mouse.y - w->oglChildPos.y;
    relativeMouseY = w->oglChildSize.y - relativeMouseY;

    if (relativeMouseX >= 0 && relativeMouseX <= w->oglChildSize.x &&
        relativeMouseY >= 0 && relativeMouseY <= w->oglChildSize.y)
    {
        int windowWidth = static_cast<int>(w->oglChildSize.x);
        int windowHeight = static_cast<int>(w->oglChildSize.y);

        w->view = w->openGLContext->getViewMatrix();
        w->proj = w->openGLContext->getProjectionMatrix();

        if (w->currentMode == &w->normalMode)
        {
            bool preventSelection = ImGuizmo::IsOver();
            if (!preventSelection)
            {
                w->selector.pickUpMesh((int)relativeMouseX, (int)relativeMouseY,
                                       windowWidth, windowHeight, w->view, w->proj, w->ThreeDObjectsList);
            }

            ThreeDObject* selected = w->selector.getSelectedObject();
            bool shiftPressed = ImGui::GetIO().KeyShift;

            if (selected)
            {
                if (!shiftPressed)
                {
                    for (auto* obj : w->ThreeDObjectsList) obj->setSelected(false);
                    w->multipleSelectedObjects.clear();
                }

                auto it = std::find(w->multipleSelectedObjects.begin(), w->multipleSelectedObjects.end(), selected);
                if (it == w->multipleSelectedObjects.end())
                {
                    w->multipleSelectedObjects.push_back(selected);
                    selected->setSelected(true);
                    //cout the position of the selected 
                    glm::vec3 worldPos = glm::vec3(selected->getGlobalModelMatrix()[3]);
                    std::cout << "[CLICK HANDLER] : Selected object world position: ("
                            << worldPos.x << ", "
                            << worldPos.y << ", "
                            << worldPos.z << ")" 
                    << " for object with name " << selected->getName() << std::endl;
                }
                else if (shiftPressed)
                {
                    w->multipleSelectedObjects.erase(it);
                    selected->setSelected(false);
                }

                if (w->objectInspector)
                {
                    if (w->multipleSelectedObjects.size() > 1)
                    {
                        w->objectInspector->clearInspectedObject();
                        w->objectInspector->setMultipleInspectedObjects(w->multipleSelectedObjects);
                    }
                    else
                    {
                        w->objectInspector->setInspectedObject(selected);
                    }
                }

                if (w->hierarchy) w->hierarchy->selectFromThreeDWindow();
                w->selector.clearTarget();
            }
            else if (!ImGuizmo::IsUsing() && !w->wasUsingGizmoLastFrame)
            {
                for (auto* obj : w->ThreeDObjectsList) obj->setSelected(false);
                w->multipleSelectedObjects.clear();
                w->selector.clearTarget();

                if (w->objectInspector)
                {
                    w->objectInspector->clearInspectedObject();
                    w->objectInspector->clearMultipleInspectedObjects();
                }

                if (w->hierarchy)
                {
                    w->hierarchy->clearMultipleSelection();
                    w->hierarchy->unselectObject(w->hierarchy->getSelectedObject());
                }

                w->wasUsingGizmoLastFrame = false;
            }
        }

        if (w->currentMode == &w->verticeMode)
        {
            if (ImGuizmo::IsUsing()) return;
            bool shiftPressed = ImGui::GetIO().KeyShift;

            Vertice* selectedVertice = w->selector.pickUpVertice(
                (int)relativeMouseX, (int)relativeMouseY,
                windowWidth, windowHeight, w->view, w->proj,
                w->ThreeDObjectsList, shiftPressed
            );

            if (selectedVertice)
            {
                if (shiftPressed)
                {
                    auto it = std::find(w->multipleSelectedVertices.begin(), w->multipleSelectedVertices.end(), selectedVertice);
                    if (it == w->multipleSelectedVertices.end())
                    {
                        w->multipleSelectedVertices.push_back(selectedVertice);
                        for (Vertice* v : w->multipleSelectedVertices) v->setSelected(true);
                    }
                }
                else
                {
                    for (ThreeDObject* obj : w->ThreeDObjectsList)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;    
                        for (Vertice* v : mesh->getVertices()) v->setSelected(false);
                    }
                    w->multipleSelectedVertices.clear();
                    selectedVertice->setSelected(true);
                    w->multipleSelectedVertices.push_back(selectedVertice);
                    w->lastSelectedVertice = selectedVertice;
                }
            }
            else
            {
                if (!shiftPressed)
                {
                    for (ThreeDObject* obj : w->ThreeDObjectsList)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;  
                        for (Vertice* vert : mesh->getVertices()) vert->setSelected(false);
                    }
                    w->multipleSelectedVertices.clear();
                    w->lastSelectedVertice = nullptr;
                }
            }
        }

        if (w->currentMode == &w->faceMode)
        {
            if (ImGuizmo::IsUsing()) return;
            bool shiftPressed = ImGui::GetIO().KeyShift;

            Face* selectedFace = w->selector.pickupFace(
                static_cast<int>(relativeMouseX), static_cast<int>(relativeMouseY),
                windowWidth, windowHeight, w->view, w->proj, w->ThreeDObjectsList, shiftPressed
            );

            if (selectedFace)
            {
                if (shiftPressed)
                {
                    auto it = std::find(w->multipleSelectedFaces.begin(), w->multipleSelectedFaces.end(), selectedFace);
                    if (it == w->multipleSelectedFaces.end())
                    {
                        w->multipleSelectedFaces.push_back(selectedFace);
                        for (Face* f : w->multipleSelectedFaces) if (f) f->setSelected(true);
                    }
                }
                else
                {
                    for (ThreeDObject* obj : w->ThreeDObjectsList)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;
                        for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
                    }
                    w->multipleSelectedFaces.clear();
                    selectedFace->setSelected(true);
                    w->multipleSelectedFaces.push_back(selectedFace);
                }
            }
            else
            {
                if (!shiftPressed)
                {
                    for (ThreeDObject* obj : w->ThreeDObjectsList)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;
                        for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
                    }
                    w->multipleSelectedFaces.clear();
                }
            }
        }

        if (w->currentMode == &w->edgeMode)
        {
            if (ImGuizmo::IsUsing()) return;
            bool shiftPressed = ImGui::GetIO().KeyShift;

            Edge* selectedEdge = w->selector.pickupEdge(
                static_cast<int>(relativeMouseX), static_cast<int>(relativeMouseY),
                windowWidth, windowHeight, w->view, w->proj, w->ThreeDObjectsList, shiftPressed
            );

            if (selectedEdge)
            {
                if (shiftPressed)
                {
                    auto it = std::find(w->multipleSelectedEdges.begin(), w->multipleSelectedEdges.end(), selectedEdge);
                    if (it == w->multipleSelectedEdges.end())
                    {
                        w->multipleSelectedEdges.push_back(selectedEdge);
                        for (Edge* e : w->multipleSelectedEdges) if (e) e->setSelected(true);
                    }
                }
                else
                {
                    for (ThreeDObject* obj : w->ThreeDObjectsList)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;
                        for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
                    }
                    w->multipleSelectedEdges.clear();
                    selectedEdge->setSelected(true);
                    w->multipleSelectedEdges.push_back(selectedEdge);
                }
            }
            else
            {
                if (!shiftPressed)
                {
                    for (ThreeDObject* obj : w->ThreeDObjectsList)
                    {
                        Mesh* mesh = dynamic_cast<Mesh*>(obj);
                        if (!mesh) continue;    
                        for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
                    }
                    w->multipleSelectedEdges.clear();
                }
            }
        }
    }
}
