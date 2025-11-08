#pragma once

#include <glad/glad.h>

class TextureRendererTest {
public:
    TextureRendererTest();
    ~TextureRendererTest();

    void initialize(int width, int height);
    
    void render();
    
    void resize(int width, int height);
    
    void setRenderRect(int x, int y, int w, int h);
    
    void updateTexture(const void* buffer, int width, int height);
    
    // Get texture ID for external rendering
    GLuint getTextureId() const { return texture_id_; }
    
    // Cleanup resources
    void cleanup();

private:
    GLuint texture_id_;
    GLuint vao_;
    GLuint vbo_;
    GLuint shader_program_;
    
    int width_;
    int height_;
    int texture_width_;
    int texture_height_;
    bool initialized_;
    
    int render_x_ = 10;
    int render_y_ = 10;
    int render_width_ = 250;
    int render_height_ = 100;

    void createTexture(int tex_width, int tex_height);
    void createQuadMesh();
    void createShaderProgram();
    GLuint compileShader(GLenum type, const char* source);
};
