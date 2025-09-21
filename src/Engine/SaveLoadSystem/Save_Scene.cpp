
#include "Save_Scene.hpp"
#include "ThirdParty/json.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>
#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"


void SaveScene::saveSceneToJson(const ThreeDScene_DNA* dna, const std::string& filePath)
{


	nlohmann::json objectTree = nlohmann::json::array();

	std::function<nlohmann::json(ThreeDObject*)> serializeHierarchy;
	serializeHierarchy = [&serializeHierarchy](ThreeDObject* obj) -> nlohmann::json {
		nlohmann::json node;
		if (!obj) return node;
		node["slot"] = obj->getSlot();
		node["object_name"] = obj->getName();
		node["id"] = obj->getID();
		const auto& children = obj->getChildren();
		if (!children.empty()) {
			nlohmann::json childrenJson = nlohmann::json::array();
			for (int j = static_cast<int>(children.size()) - 1; j >= 0; --j) 
			{
				auto it = children.begin();
				std::advance(it, j);
				childrenJson.push_back(serializeHierarchy(*it));
			}
			node["children"] = childrenJson;
		}
		return node;
	};

	std::vector<ThreeDObject*> roots;
	const auto& history = dna->getHistory();
	for (int e = static_cast<int>(history.size()) - 1; e >= 0; --e) 
	{
		const auto& event = history[e];
		if (event.kind == SceneEventKind::InitSnapshot) {
			for (int i = static_cast<int>(event.initPtrs.size()) - 1; i >= 0; --i) 
			{
				ThreeDObject* obj = event.initPtrs[i];
				if (obj && !obj->getParent()) {
					if (std::find(roots.begin(), roots.end(), obj) == roots.end())
						roots.push_back(obj);
				}
			}
		}
		if (event.kind == SceneEventKind::AddObject) {
			if (event.ptr && !event.ptr->getParent()) {
				if (std::find(roots.begin(), roots.end(), event.ptr) == roots.end())
					roots.push_back(event.ptr);
			}
		}
	}

	for (int i = static_cast<int>(roots.size()) - 1; i >= 0; --i) 
	{
		objectTree.push_back(serializeHierarchy(roots[i]));
	}

	nlohmann::json outputJson = nlohmann::json::object();
	outputJson["object_tree"] = objectTree;
	outputJson["scene_id"] = activeScene["scene_id"];

	std::ofstream file(filePath);
	if (file.is_open())
	{
		file << outputJson.dump(4);
		file.close();
		std::cout << "[SaveScene] Scene successfully saved to: " << filePath << std::endl;
	}
	else
	{
		throw std::runtime_error("Impossible d'ouvrir le fichier pour la sauvegarde.");
	}
}