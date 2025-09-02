#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "WorldObjects/Camera/Camera.hpp"
#include <iostream>

class OpenGLContext
{
public:
    OpenGLContext();
    ~OpenGLContext() = default;

    void initialize();
    void resize(int w, int h);

    void bindForRendering();
    void unbind();


    GLuint getFbo() const { return fbo; }
    GLuint getTexture() const { return fboTexture; }
    int getWidth()  const { return width; }
    int getHeight() const { return height; }

private:
    GLuint fbo;
    GLuint fboTexture;
    GLuint rbo;

    int width  = 800;
    int height = 600;

};
