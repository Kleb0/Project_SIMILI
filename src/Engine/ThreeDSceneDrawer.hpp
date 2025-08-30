#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Engine/Guizmo.hpp"
#include <WorldObjects/Entities/ThreeDObject.hpp>
#include "WorldObjects/Camera/Camera.hpp"
#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
#include <iostream>
#include <list>

class ThreeDSceneDrawer
{
public:
    ThreeDSceneDrawer();
    ~ThreeDSceneDrawer() = default;

    void initizalize();

    ThreeDScene_DNA* getSceneDNA() const { return sceneDNA; }
    void setSceneDNA(ThreeDScene_DNA* dna, bool takeOwnership = true);

    void resize(int w, int h);
    void render(const std::list<ThreeDObject *> &objects, const glm::mat4 &viewProj);
    void drawBackgroundGradient();

    GLuint getTexture() const { return fboTexture; }
    std::vector<glm::vec3> worldCenter;


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

    ThreeDScene_DNA* sceneDNA{nullptr};
    bool ownsSceneDNA{true};
};
