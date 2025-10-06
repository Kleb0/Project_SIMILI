#pragma once

#include <vector>
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Quad.hpp"

namespace MeshEdit 
{
	void CutQuad(const std::vector<Edge*>& loop, Mesh* mesh, const std::vector<Quad*>& traversedQuads);

}
