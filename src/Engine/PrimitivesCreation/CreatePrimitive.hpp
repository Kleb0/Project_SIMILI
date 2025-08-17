#pragma once
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
#include <glm/glm.hpp>
#include <string>

namespace Primitives
{

    Mesh* CreateCubeMesh(
        float size = 1.0f,
        const glm::vec3& center = glm::vec3(0.0f),
        const std::string& name = "Cube",
        bool attachDNA = true
    );
}
