#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <WorldObjects/ThreeDObject.hpp>
#include "WorldObjects/Camera.hpp"
#include <iostream>
#include <list>

class ThreeDSceneDrawer
{
public:
    ThreeDSceneDrawer();
    ~ThreeDSceneDrawer() = default;

    void initizalize();
    void resize(int w, int h);
    void render(const std::list<ThreeDObject *> &objects, const glm::mat4 &viewProj);
    void drawBackgroundGradient();

    void add(ThreeDObject *object);
    GLuint getTexture() const { return fboTexture; }

private:
    std::list<ThreeDObject *> objects;
    Camera camera;

    GLuint fbo = 0;
    GLuint fboTexture = 0;
    GLuint rbo = 0;
    int width = 800;
    int height = 600;

    void compileShaders();
    GLuint gridVAO = 0;
    GLuint gridVBO = 0;
    GLuint shaderProgram = 0;
};
