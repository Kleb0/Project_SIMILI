#define GLM_ENABLE_EXPERIMENTAL

#include "Engine/ThreeDInteractions/MeshTransform.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/Guizmo.hpp"

#include "WorldObjects/Mesh/Mesh.hpp" 
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"


#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Vertice_Mode.hpp"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/epsilon.hpp>
#include <iostream>
#include <algorithm>

namespace MeshTransform
{
    static uint64_t gMeshDNATick = 0;

    void manipulateChildrens(ThreeDObject* parent, const glm::mat4& delta)
    {
        for (ThreeDObject* child : parent->getChildren())
        {
            glm::mat4 localModel = child->getModelMatrix();
            glm::mat4 newLocalModel = delta * localModel;
            child->setModelMatrix(newLocalModel);

            glm::vec3 parentWorldOrigin = glm::vec3(parent->getGlobalModelMatrix() * glm::vec4(parent->getOrigin(), 1.0f));
            glm::mat4 childGlobalAfter = child->getGlobalModelMatrix();
            glm::vec3 newLocalOrigin = glm::vec3(glm::inverse(childGlobalAfter) * glm::vec4(parentWorldOrigin, 1.0f));
            child->setOrigin(newLocalOrigin);

            if (child->canHaveChildren)
                manipulateChildrens(child, delta);
        }
    }

    void applyGizmoTransformation(const glm::mat4& delta, const std::list<ThreeDObject*>& selectedObjects, ImGuizmo::OPERATION op)
    {
        const float epsilon = 1e-5f;
        const glm::mat4 I(1.0f);
        bool isIdentity = true;

        for (int c = 0; c < 4 && isIdentity; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                if (!glm::epsilonEqual(delta[c][r], I[c][r], epsilon))
                {
                    isIdentity = false;
                    break;
                }
            }
        }

        if (isIdentity)
            return;

        for (ThreeDObject* obj : selectedObjects)
        {
            if (obj->getParent())
            {
                glm::mat4 newLocal = delta * obj->getModelMatrix();
                obj->setModelMatrix(newLocal);
            }
            else
            {
                if (op == ImGuizmo::SCALE)
                {
                    // glm::mat4 localDelta = glm::inverse(obj->getGlobalModelMatrix()) * delta * obj->getGlobalModelMatrix();

                    // glm::vec3 deltaScale;
                    // deltaScale.x = glm::length(glm::vec3(localDelta[0]));
                    // deltaScale.y = glm::length(glm::vec3(localDelta[1]));
                    // deltaScale.z = glm::length(glm::vec3(localDelta[2]));

                    // glm::vec3 currentScale = obj->getScale();
                    // glm::vec3 newScale = currentScale * deltaScale;

                    // // Reconstruct TRS
                    // glm::mat4 model = glm::mat4(1.0f);
                    // model = glm::translate(model, obj->getPosition());
                    // model *= glm::toMat4(obj->rotation);
                    // model = glm::scale(model, newScale);

                    // glm::vec3 origin = obj->getOrigin();
                    // glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), origin);
                    // glm::mat4 back = glm::translate(glm::mat4(1.0f), -origin);

                    // glm::mat4 finalModel = toOrigin * model * back;
                    // obj->setModelMatrix(finalModel);
                    scaleCleanup(obj, delta);

                }

                else
                {
                    glm::vec3 origin = obj->getOrigin();
                    glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), origin);
                    glm::mat4 back = glm::translate(glm::mat4(1.0f), -origin);

                    glm::mat4 newGlobal = toOrigin * delta * back * obj->getGlobalModelMatrix();
                    obj->setGlobalModelMatrix(newGlobal);
                }
            }

            if (obj->canHaveChildren)
                manipulateChildrens(obj, delta);
        }
    }


    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op,OpenGLContext* context, const std::list<ThreeDObject*>& selectedObjects, 
    const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
        glm::mat4 view = context->getViewMatrix();
        glm::mat4 proj = context->getProjectionMatrix();

        return Guizmo::renderGizmoForObject(selectedObjects, op, view, proj, oglChildPos, oglChildSize);
    }

    void MeshTransform::manipulateMesh(OpenGLContext* context, const std::list<ThreeDObject*>& selectedObjects,
    const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame)
    {
        
        static bool firstOpDetected = false;
        static bool didPrintScaleTest = false;
        static bool didPrintRotationAfterScale = false;
        static bool hasRotatedOnce = false;
        static std::vector<glm::vec3> initialPositions;

        if (selectedObjects.empty()) return;

        static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

        glm::mat4 view = context->getViewMatrix();
        glm::mat4 proj = context->getProjectionMatrix();

        glm::mat4 dummyMatrix = Guizmo::renderGizmoForObject(selectedObjects, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);

        static glm::mat4 startMatrix = glm::mat4(1.0f);
        static glm::mat4 prevMatrix  = glm::mat4(1.0f);
        static bool gizmoActive = false;

        const bool isUsing = ImGuizmo::IsUsing();

        if (isUsing && !gizmoActive)
        {
            startMatrix = dummyMatrix;
            prevMatrix  = dummyMatrix;
            gizmoActive = true;

            if (!firstOpDetected)
            {
                if (currentGizmoOperation == ImGuizmo::SCALE)
                {
                    std::cout << "TEST FIRST OPERATION IS SCALE" << std::endl;
                    didPrintScaleTest = true;
                }

                firstOpDetected = true;
            }
        }

        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
        currentGizmoOperation, ImGuizmo::WORLD, glm::value_ptr(dummyMatrix)))
        {
            glm::mat4 delta = dummyMatrix * glm::inverse(prevMatrix);
            applyGizmoTransformation(delta, selectedObjects, currentGizmoOperation);
            prevMatrix = dummyMatrix;
            wasUsingGizmoLastFrame = true;
        }

        if (!isUsing && gizmoActive)
        {
            glm::mat4 totalDelta = dummyMatrix * glm::inverse(startMatrix);
            trackMeshTransformOnRelease(selectedObjects, totalDelta, currentGizmoOperation);

            if (currentGizmoOperation == ImGuizmo::ROTATE && !hasRotatedOnce)
            {
                std::cout << "Rotation done" << std::endl;
                hasRotatedOnce = true;
            }

            // ----- uncoment if output needed
            // if (didPrintScaleTest && currentGizmoOperation == ImGuizmo::SCALE)
            // {
            //     outputTransform(selectedObjects, initialPositions, hasRotatedOnce, didPrintRotationAfterScale);
            // }
            // ---------------------------------------------------

            gizmoActive = false;
            firstOpDetected = false;
            didPrintScaleTest = false;
            didPrintRotationAfterScale = false;
            initialPositions.clear();
        }
        wasUsingGizmoLastFrame = isUsing;
    }

    void MeshTransform::trackMeshTransformOnRelease(const std::list<ThreeDObject*>& selectedObjects,
    const glm::mat4& totalDelta, ImGuizmo::OPERATION op)
    {
        const glm::mat4 I(1.0f);
        bool isIdentity = true;
        for (int c = 0; c < 4 && isIdentity; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                if (!glm::epsilonEqual(totalDelta[c][r], I[c][r], 1e-6f))
                {
                    isIdentity = false;
                    break;
                }
            }
        }
        if (isIdentity) return;

        std::string tag = "unknown";
        switch (op)
        {
            case ImGuizmo::TRANSLATE: tag = "translate"; break;
            case ImGuizmo::ROTATE: tag = "rotate"; break;
            case ImGuizmo::SCALE: tag = "scale"; break;
            default: break;
        }

        for (ThreeDObject* obj : selectedObjects)
        {
            if (auto* mesh = dynamic_cast<Mesh*>(obj))
            {
                if (auto* dna = mesh->getMeshDNA())
                {
                    dna->trackWithAutoTick(totalDelta, tag);
                }
            }
        }
    }


    // it appears that ImGuizmo Scale is bugged so i made a correction to apply to the pivot here
    void scaleCleanup(ThreeDObject* obj, const glm::mat4& delta)
    {
        glm::mat4 localDelta = glm::inverse(obj->getGlobalModelMatrix()) * delta * obj->getGlobalModelMatrix();

        glm::vec3 deltaScale;
        deltaScale.x = glm::length(glm::vec3(localDelta[0]));
        deltaScale.y = glm::length(glm::vec3(localDelta[1]));
        deltaScale.z = glm::length(glm::vec3(localDelta[2]));

        glm::vec3 currentScale = obj->getScale();
        glm::vec3 newScale = currentScale * deltaScale;

        // Reconstruct TRS
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, obj->getPosition());
        model *= glm::toMat4(obj->rotation); 
        model = glm::scale(model, newScale);


        glm::vec3 origin = obj->getOrigin();
        glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), origin);
        glm::mat4 back     = glm::translate(glm::mat4(1.0f), -origin);

        glm::mat4 finalModel = toOrigin * model * back;
        obj->setModelMatrix(finalModel);
    }


    void outputTransform(const std::list<ThreeDObject*>& selectedObjects,
    const std::vector<glm::vec3>& initialPositions, bool hasRotatedOnce, bool& didPrintRotationAfterScale)
    {
        std::cout << "Comparing positions after scale:\n";
        int i = 0;

        for (ThreeDObject* obj : selectedObjects)
        {
            glm::vec3 newPos = glm::vec3(obj->getGlobalModelMatrix()[3]);
            const glm::vec3& oldPos = (i < initialPositions.size()) ? initialPositions[i] : glm::vec3(0.0f);

            std::string objectName = obj->getName();
            std::cout << "Object " << i << " (Name: " << objectName << "):\n";
            std::cout << "  Old position: (" << oldPos.x << ", " << oldPos.y << ", " << oldPos.z << ")\n";
            std::cout << "  New position: (" << newPos.x << ", " << newPos.y << ", " << newPos.z << ")\n";
            ++i;
        }

        std::cout << "Comparing rotation after scale and after first rotation is done :\n";

        if (hasRotatedOnce && !didPrintRotationAfterScale)
        {
            std::cout << "\nRotation after scale and first rotation:\n";
            for (ThreeDObject* obj : selectedObjects)
            {
                glm::vec3 rot = obj->getRotation();
                std::cout << "Object: " << obj->getName() << " - Rotation (degrees): "
                        << "(" << rot.x << ", " << rot.y << ", " << rot.z << ")\n";
            }
            didPrintRotationAfterScale = true;
        }
    }


}