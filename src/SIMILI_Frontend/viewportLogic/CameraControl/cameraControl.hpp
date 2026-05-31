#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

class VKScene;
class Camera;

class CameraControl {
public:
    CameraControl();
    ~CameraControl();

    void setScene(VKScene* scene);

    void onMiddleButtonDown(int mouseX, int mouseY);
    void onMiddleButtonUp();
    void onMouseMove(int mouseX, int mouseY);
    void onWheel(float wheelDelta);

private:
    void prepareOrbit(Camera* cam);
    void resetOrbitPreparation();
    void orbitAroundTarget(Camera* cam, float deltaX, float deltaY);
    void lateralMovement(Camera* cam, float deltaX, float deltaY);

    VKScene* scene_;
    bool is_dragging_;
    int last_mouse_x_;
    int last_mouse_y_;

    float yaw_;
    float pitch_;
    float orbit_radius_;
    bool is_orbit_prepared_;
};
