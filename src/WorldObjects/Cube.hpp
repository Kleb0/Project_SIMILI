#pragma once

#include "WorldObjects/ThreedObject.hpp"

class Cube : public ThreeDObject
{
public:
    Cube();
    ~Cube();

    void initialize() override;
    void render(const glm::mat4 &viewProj) override;

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    void compileShaders();
};
