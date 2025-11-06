#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <string>

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
    std::string getContextID() const { return contextID; }

private:
    GLuint fbo;
    GLuint fboTexture;
    GLuint rbo;

    int width  = 800;
    int height = 600;
    
    std::string contextID;
    
    std::string generateContextID();

};
