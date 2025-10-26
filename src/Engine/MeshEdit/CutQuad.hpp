#pragma once

#include <vector>
#include <unordered_set>
#include <string>
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Quad.hpp"

namespace MeshEdit 
{
	void CutQuad(const std::vector<Edge*>& loop, Mesh* mesh, const std::vector<Quad*>& traversedQuads);

	void findFirstQuadEdgesWithVertice(const std::vector<class Vertice*>& centers, const std::unordered_set<std::string>& centerEdgeIDs, 
	 const std::vector<Edge*>& centerEdges, Mesh* mesh, std::vector<Edge*>& firstRowEdges, std::vector<Edge*>& firstQuadEdges);

	void findNextQuadEdgesForFirstRow(const std::vector<class Vertice*>& centers, const std::unordered_set<std::string>& centerEdgeIDs, 
	const std::vector<Edge*>& centerEdges, Mesh* mesh, std::vector<Edge*>& QuadEdges, const glm::vec3& refDir, int currentIndex);

	void buildQuadFromEdges(const std::vector<Edge*>& QuadEdges, class Vertice* currentVert, class Vertice* nextVert, Mesh* mesh);
	
}
