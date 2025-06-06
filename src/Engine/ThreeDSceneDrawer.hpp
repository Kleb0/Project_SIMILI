#pragma once

#include <vector>
#include "WorldObjects/ThreedObject.hpp"

class ThreeDSceneDrawer
{
public:
    ThreeDSceneDrawer();
    void render(const std::vector<ThreeDObject *> &objects, const glm::mat4 &viewProj);
    void initialization();
    void drawBackgroundGradient();
    void add(ThreeDObject &object);

private:
    std::vector<ThreeDObject *> objects;
};
