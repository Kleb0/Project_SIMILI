#pragma once
#include <glad/glad.h>
#include "Engine/ThreeDSceneDrawer.hpp"
#include "WorldObjects/ThreedObject.hpp"
#include "WorldObjects/Camera.hpp"
#include <iostream>
#include <list>

class OpenGLContext
{
public:
    OpenGLContext();
    ~OpenGLContext() = default;

    void initialize();
    void render();
    GLuint getTexture() const { return fboTexture; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }

    void setCamera(Camera *cam) { camera = cam; }
    Camera *getCamera() const { return camera; }

    const glm::mat4 &getViewMatrix() const { return viewMatrix; }
    const glm::mat4 &getProjectionMatrix() const { return projMatrix; }

    std::list<ThreeDObject *> objects;

    void addThreeDObjectToList(ThreeDObject *object);

    // std::list<ThreeDObject *> getObjects() const
    // {
    //     return objects;
    // }
    const std::list<ThreeDObject *> &getObjects() const { return objects; } 
    std::list<ThreeDObject *> &getObjects() { return objects; } 

    void setListOfObjects(std::list<ThreeDObject *> &list)
    {
        objects = list;
    }

    void removeThreeDobjectFromList(ThreeDObject *object);

private:
    glm::mat4 viewMatrix = glm::mat4(1.0f);
    glm::mat4 projMatrix = glm::mat4(1.0f);

    GLuint fbo = 0;
    GLuint fboTexture = 0;
    GLuint rbo = 0;
    int width = 800;
    int height = 600;

    ThreeDSceneDrawer scene;
    Camera *camera = nullptr;
};