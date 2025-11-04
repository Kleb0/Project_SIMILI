#define GLM_ENABLE_EXPERIMENTAL

#include <glad/glad.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "UI/MainSoftwareGUI.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "UI/ObjectInspectorLogic/ObjectInspector.hpp"

#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"

#include "WorldObjects/Mesh/Mesh.hpp"

// #include <SDL3/SDL.h>  // Not used in CEF UI project
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <algorithm>
#include <glm/gtx/string_cast.hpp>

#include "Engine/ThreeDScene.hpp"
// #include <windows.h>

ThreeDWindow::ThreeDWindow() : clickHandler(this) {}

// ------- Constructor, setters and getters ------- //


ThreeDWindow::ThreeDWindow(const std::string &title, const std::string &text)
    : title(title), text(text), clickHandler(this)
{
}

ThreeDWindow &ThreeDWindow::setRenderer(OpenGLContext &context)
{
    openGLContext = &context;
    return *this;
}

void ThreeDWindow::setModelingMode(ThreeDMode* mode)
{
    if (mode)
    {
        currentMode = mode;
    }
}


ThreeDObject *ThreeDWindow::getSelectedObject() const
{
    return selector.getSelectedObject();
}


// the threeDWindow use external components, we set them in the main script

void ThreeDWindow::setMainGUI(MainSoftwareGUI* gui)
{
    mainGUI = gui;
}


void ThreeDWindow::setHierarchy(HierarchyInspector *inspector)
{
    hierarchy = inspector;
}

void ThreeDWindow::setObjectInspector(ObjectInspector *inspector)
{
    objectInspector = inspector;
}

// ----- Clearing ------ //

void ThreeDWindow::clearSelectedFaces()
{
    multipleSelectedFaces.clear();
}

// ------- Rendering the ThreeDWindow ------- //

void ThreeDWindow::render()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    ImGui::Begin(title.c_str(), nullptr, flags);

    ImGui::Text("%s", text.c_str());

    if (scene)
    {
        onChangeMod();
        threeDRendering();
    }
    else
    {
        ImGui::TextUnformatted("[ThreeDWindow] No ThreeDScene attached.");
    }

    ImGui::End();
}

void ThreeDWindow::threeDRendering()
{

    ImGui::BeginChild("3D Scene Window", ImVec2(0, 0), true, ImGuiWindowFlags_None);

    oglChildPos = ImGui::GetCursorScreenPos();
    oglChildSize = ImGui::GetContentRegionAvail();

    int newWidth = static_cast<int>(oglChildSize.x);
    int newHeight = static_cast<int>(oglChildSize.y);

    scene->render();
    ImTextureID textureID = (ImTextureID)(intptr_t)scene->getTexture();

    ImGui::Image(textureID, oglChildSize, ImVec2(0, 1), ImVec2(1, 0));

    ThreeDWorldInteractions();

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGui::GetCurrentWindow()->Flags |= ImGuiWindowFlags_NoMove;
        clickHandler.handle();
    }

    static bool isDragging = false;
    static ImVec2 lastMousePos;

    if (ImGui::IsWindowHovered())
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            Camera* cam = scene ? scene->getActiveCamera() : nullptr;
            if (cam && cam->isSoftwareCamera())
            {
                cam->moveForward(wheel * 0.5f);
            }
        }

        // ------- Camera controls ---------- //
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            ImVec2 currentMousePos = ImGui::GetMousePos();

            Camera* cam = scene ? scene->getActiveCamera() : nullptr;
            if (cam && cam->isSoftwareCamera())
            {
                if (!isDragging)
                {
                    lastMousePos = currentMousePos;
                    isDragging = true;
                }

                float deltaX = currentMousePos.x - lastMousePos.x;
                float deltaY = currentMousePos.y - lastMousePos.y;

                if (deltaX != 0.0f || deltaY != 0.0f)
                {
                    if (!ImGui::GetIO().KeyShift)
                        cam->prepareOrbit();

                    if (ImGui::GetIO().KeyShift)
                        cam->lateralMovement(deltaX, deltaY);
                    else
                        cam->orbitAroundTarget(deltaX, deltaY);
                }

                lastMousePos = currentMousePos;
            }
        }
        else
        {
            isDragging = false;
        }
    }

    renderModelingModes();

    ImGui::EndChild();

}

void ThreeDWindow::renderModelingModes()
{
    ImGui::SetCursorScreenPos(ImVec2(oglChildPos.x + 10, oglChildPos.y + 10));
    ImGui::BeginGroup();

    ImVec2 iconSize = ImVec2(32, 32);
    ImVec2 iconPos0 = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Icon 1 : Normal Mode
    ImVec2 min0 = iconPos0;
    ImVec2 max0 = ImVec2(min0.x + iconSize.x, min0.y + iconSize.y);

    ImU32 normalBorderColor = (currentMode == &normalMode)
        ? IM_COL32(255, 165, 0, 255)
        : IM_COL32(255, 255, 255, 255);

    ImU32 normalTextColor = (currentMode == &normalMode)
        ? IM_COL32(255, 200, 100, 255)
        : IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(min0, max0, IM_COL32(0, 0, 0, 0));
    draw_list->AddRect(min0, max0, normalBorderColor, 0.0f, 0, 2.0f);
    draw_list->AddText(ImVec2(min0.x + 10, min0.y + 6), normalTextColor, "1");

    // === Icon 2 : Vertice Mode ===
    ImGui::SetCursorScreenPos(ImVec2(min0.x + iconSize.x + 5, min0.y));
    ImVec2 iconPos1 = ImGui::GetCursorScreenPos();
    ImVec2 min1 = iconPos1;
    ImVec2 max1 = ImVec2(min1.x + iconSize.x, min1.y + iconSize.y);

    ImU32 verticeBorderColor = (currentMode == &verticeMode)
        ? IM_COL32(255, 165, 0, 255)
        : IM_COL32(255, 255, 255, 255);

    ImU32 verticeTextColor = (currentMode == &verticeMode)
        ? IM_COL32(255, 200, 100, 255)
        : IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(min1, max1, IM_COL32(0, 0, 0, 0));
    draw_list->AddRect(min1, max1, verticeBorderColor, 0.0f, 0, 2.0f);
    draw_list->AddText(ImVec2(min1.x + 10, min1.y + 6), verticeTextColor, "2");

    // --- End of Icon 2 ---

    // === Icon 3 : Face Mode ===
    ImGui::SetCursorScreenPos(ImVec2(iconPos0.x + 2 * (iconSize.x + 5), iconPos0.y)); // 3rd case
    ImVec2 iconPos2 = ImGui::GetCursorScreenPos();
    ImVec2 min2 = iconPos2;
    ImVec2 max2 = ImVec2(min2.x + iconSize.x, min2.y + iconSize.y);

    ImU32 faceBorderColor = (currentMode == &faceMode)
        ? IM_COL32(255, 165, 0, 255)
        : IM_COL32(255, 255, 255, 255);

    ImU32 faceTextColor = (currentMode == &faceMode)
        ? IM_COL32(255, 200, 100, 255)
        : IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(min2, max2, IM_COL32(0, 0, 0, 0));
    draw_list->AddRect(min2, max2, faceBorderColor, 0.0f, 0, 2.0f);
    draw_list->AddText(ImVec2(min2.x + 10, min2.y + 6), faceTextColor, "3");
    // --- End of Icon 3 ---

    // === Icon 4 : Edge Mode ===
    ImGui::SetCursorScreenPos(ImVec2(iconPos0.x + 3 * (iconSize.x + 5), iconPos0.y)); // 4th case
    ImVec2 iconPos3 = ImGui::GetCursorScreenPos();
    ImVec2 min3 = iconPos3;
    ImVec2 max3 = ImVec2(min3.x + iconSize.x, min3.y + iconSize.y);

    ImU32 edgeBorderColor = (currentMode == &edgeMode)
        ? IM_COL32(255, 165, 0, 255)
        : IM_COL32(255, 255, 255, 255);

    ImU32 edgeTextColor = (currentMode == &edgeMode)
        ? IM_COL32(255, 200, 100, 255)
        : IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(min3, max3, IM_COL32(0, 0, 0, 0));
    draw_list->AddRect(min3, max3, edgeBorderColor, 0.0f, 0, 2.0f);
    draw_list->AddText(ImVec2(min3.x + 10, min3.y + 6), edgeTextColor, "4");
    // ---- Icon 4 : Edge Mode ----

    float textX = max3.x + 15;
    float textY = min3.y + 6;
    ImGui::SetCursorScreenPos(ImVec2(textX, textY));
    const char* modeText = "";

    if (currentMode == &normalMode)
        modeText = "-- Normal mode";
    else if (currentMode == &verticeMode)
        modeText = "-- Vertice mode";
    else if (currentMode == &faceMode)
        modeText = "-- Face mode";
    else if (currentMode == &edgeMode)
        modeText = "-- Edge mode";

    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), modeText);

    ImGui::EndGroup();
    draw_list->AddText(ImVec2(min3.x + 10, min3.y + 6), edgeTextColor, "4");

    // ---- Icon 4 : Edge Mode + text ----

    ImGui::EndGroup();
}

void ThreeDWindow::onChangeMod()
{
    ImGuiIO& io = ImGui::GetIO();
    bool isPressed1 = ImGui::IsKeyDown(ImGuiKey_1) || ImGui::IsKeyDown(ImGuiKey_Keypad1);
    bool isPressed2 = ImGui::IsKeyDown(ImGuiKey_2) || ImGui::IsKeyDown(ImGuiKey_Keypad2);
    bool isPressed3 = ImGui::IsKeyDown(ImGuiKey_3) || ImGui::IsKeyDown(ImGuiKey_Keypad3);

    // i don't know why but ImGUI can't read key 4 so we pass by the key apostrophe in Edge Mode.
    // think for later : Try to implemente a CFG file that can read inputs from Qwerty or Azerty
    bool isPressed4 = ImGui::IsKeyDown(ImGuiKey_Apostrophe) || ImGui::IsKeyDown(ImGuiKey_Keypad4);

    if (isPressed1  && !lastKeyState_1)
    {
        setModelingMode(&normalMode);
        this->mainGUI->SetCurrentMode(currentMode);
    }

    if (isPressed2 && !lastKeyState_2)
    {
        setModelingMode(&verticeMode);
        this->mainGUI->SetCurrentMode(currentMode);
    }

    if (isPressed3 && !lastKeyState_3)
    {
        setModelingMode(&faceMode);
        this->mainGUI->SetCurrentMode(currentMode);
    }

    if (isPressed4 && !lastKeyState_4)
    {
        setModelingMode(&edgeMode);
        this->mainGUI->SetCurrentMode(currentMode);

    }

    lastKeyState_1 = isPressed1;
    lastKeyState_2 = isPressed2;
    lastKeyState_3 = isPressed3;
    lastKeyState_4 = isPressed4;

}

// --------- Object Manipulation ----------- //
void ThreeDWindow::ThreeDWorldInteractions()
{
    if(currentMode == &normalMode)
    {
        MeshTransform::manipulateMesh(
            scene,
            multipleSelectedObjects,
            oglChildPos,
            oglChildSize,
            wasUsingGizmoLastFrame
        );
    }

    if(currentMode == &verticeMode)
    {
        VerticeTransform::manipulateVertices
        (
            scene,
            multipleSelectedVertices,
            oglChildPos,
            oglChildSize,
            wasUsingGizmoLastFrame
        );
        
    }

    if(currentMode == &faceMode)
    {
        FaceTransform::manipulateFaces
        (
            scene,
            multipleSelectedFaces,
            oglChildPos,
            oglChildSize,
            wasUsingGizmoLastFrame,
            true
        );

    }

    if(currentMode == &edgeMode)
    {
        EdgeTransform::manipulateEdges
        (
            scene,
            multipleSelectedEdges,
            oglChildPos,
            oglChildSize,
            wasUsingGizmoLastFrame,
            this
        );    
    }
}


void ThreeDWindow::setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects)
{
    multipleSelectedObjects = objects;
}