#pragma once

#include <glad/glad.h>
#include "Engine/ThreeDSceneDrawer.hpp"
#include "WorldObjects/Camera.hpp"
#include "WorldObjects/ThreeDObject.hpp"
#include <vector>

class ThreeDWorldView
{
public:
    ThreeDWorldView();
    ~ThreeDWorldView() = default;

    void initialize();
    void resize(int w, int h);
    void render();
    GLuint getTexture() const { return fboTexture; }

    std::vector<ThreeDObject *> objects;
    void add(ThreeDObject *obj) { objects.push_back(obj); }

private:
    GLuint fbo = 0;
    GLuint fboTexture = 0;
    GLuint rbo = 0;
    int width = 800;
    int height = 600;

    ThreeDSceneDrawer scene;
    Camera camera;
};
