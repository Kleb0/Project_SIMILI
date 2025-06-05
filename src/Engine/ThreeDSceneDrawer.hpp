#pragma once

#include <vector>
#include "WorldObjects/ThreedObject.hpp"

class ThreeDSceneDrawer
{
public:
    ThreeDSceneDrawer();
    void render(const glm::mat4 &viewProj);
    void initialization();
    void drawBackgroundGradient();
    void add(ThreeDObject &object);

private:
    std::vector<ThreeDObject *> objects;
};
