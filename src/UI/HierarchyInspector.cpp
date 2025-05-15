#include "UI/HierarchyInspector.hpp"
#include <imgui.h>
#include <iostream>
#include <algorithm>

HierarchyInspector::HierarchyInspector() {}

void HierarchyInspector::setContext(OpenGLContext *ctxt)
{
    context = ctxt;
}

void HierarchyInspector::setThreeDWindow(ThreeDWindow *win)
{
    window = win;
}

void HierarchyInspector::render()
{
    ImGui::Begin("Hierarchy Inspector");
    ImGui::Text("List of objects in the current 3D scene :");

    if (context)
    {
        for (ThreeDObject *obj : context->getObjects())
        {
            if (obj)
                ImGui::BulletText("%s", obj->getName().c_str());
        }
    }

    if (window)
    {
        for (ThreeDObject *obj : window->getObjects())
        {

            // avoid displaying the same object twice a time
            if (context && std::find(context->getObjects().begin(), context->getObjects().end(), obj) != context->getObjects().end())
                continue;

            if (obj)
                ImGui::BulletText("%s", obj->getName().c_str());
        }
    }

    ImGui::End();
    S
}