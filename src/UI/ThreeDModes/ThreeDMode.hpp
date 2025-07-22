#pragma once


#include <vector>

class ThreeDWindow;

class ThreeDMode {
public:
    virtual ~ThreeDMode() = default;

    virtual void activate(ThreeDWindow* window) = 0;
    virtual void deactivate(ThreeDWindow* window) = 0;
    virtual void update(ThreeDWindow* window, float deltaTime) = 0;
    virtual void onKeyPress(ThreeDWindow* window, int key, int scancode, int action, int mods) {}
    virtual const char* getName() const = 0;
};
