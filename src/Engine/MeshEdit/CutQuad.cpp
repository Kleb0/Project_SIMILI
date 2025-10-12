#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include "Engine/ErrorBox.hpp"

namespace MeshEdit 
{

	void CutQuad(const std::vector<Edge*>& loop, Mesh* mesh, const std::vector<Quad*>& traversedQuads)
	{
		std::vector<Vertice*> centers;
		std::vector<Edge*> traversedEdges;
		
		std::vector<std::array<Vertice*, 3>> allVertsA;
		std::vector<std::array<Vertice*, 3>> allVertsB;

		static int cutCount = 0;
		static int edgeToCutCount = 0;

		for(int i = 0; i < traversedQuads.size(); i++)
		{
			cutCount++;
		}

		for(int i = 0; i < loop.size(); i++)
		{
			edgeToCutCount++;
		}

		std::cout << "[Cut quad] : found " << cutCount << " quads to cut " << std::endl;
		std::cout << "[Cut quad] : found " << edgeToCutCount << " edges to cut the loop " << std::endl;

		cutCount = 0;
		edgeToCutCount = 0;

		
	}
}