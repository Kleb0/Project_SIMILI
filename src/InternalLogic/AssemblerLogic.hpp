#pragma once
#include <iostream>

#include "UI/MainSoftwareGUI.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/ThreedObject.hpp"
#include "UI/GUIWindow.hpp"
#include "UI/InfoWindow.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "UI/ObjectInspector.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"

#define Add associate

inline MainSoftwareGUI &associate(MainSoftwareGUI &gui, GUIWindow &win)
{
    return gui.add(win);
}

inline ThreeDWindow &associate(ThreeDWindow &win, ThreeDObject &obj)
{
    win.addThreeDObjectsToScene({&obj});
    return win;
}

inline OpenGLContext &associate(OpenGLContext &ctx, ThreeDObject &obj)
{
    ctx.addThreeDObjectToList(&obj);
    return ctx;
}

inline MainSoftwareGUI &associate(MainSoftwareGUI &gui, InfoWindow &win)
{
    return gui.add(win);
}

inline MainSoftwareGUI &associate(MainSoftwareGUI &gui, ThreeDWindow &win)
{
    return gui.add(win);
}

inline MainSoftwareGUI &associate(MainSoftwareGUI &gui, ObjectInspector &win)
{
    return gui.add(win);
}

inline MainSoftwareGUI &associate(MainSoftwareGUI &gui, HierarchyInspector &win)
{
    return gui.add(win);
}

template <typename LeftType, typename RightType>
LeftType &associate(LeftType &left, RightType &right)
{
    static_assert(sizeof(LeftType) == 0,
                  "No overload of 'associate' is defined for this pair of types. "
                  "Please provide a matching associate(LeftType&, RightType&) function.");
    return left;
}
