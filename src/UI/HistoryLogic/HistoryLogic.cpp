#include "UI/HistoryLogic/HistoryLogic.hpp"
#include "UI/ObjectInspectorLogic/ObjectInspector.hpp" 
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp" 
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDScene.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp" 
#include "Engine/ThreeDInteractions/MeshTransform.hpp" 
#include <imgui.h>

#include <glm/gtc/matrix_inverse.hpp> 
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/common.hpp> 


HistoryLogic::HistoryLogic() {}

void HistoryLogic::setTitle(const std::string& t) { title = t; }
void HistoryLogic::setText(const std::string& t)  { content = t; }

void HistoryLogic::render()
{
	//---------//
	ImGui::PushStyleColor(ImGuiCol_TitleBg, IM_COL32(0,128,0,255));
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive,IM_COL32(0,200,0,255));
	ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.00f,0.50f,0.00f,1.0f));
	ImGui::PushStyleColor(ImGuiCol_TabHovered,ImVec4(0.00f,0.70f,0.00f,1.0f));
	ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.00f,0.80f,0.00f,1.0f));
	ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive,ImVec4(0.00f,0.60f,0.00f,1.0f));

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,0,0,255));
	ImGui::Begin(title.c_str());
	ImGui::PopStyleColor();

	ImGui::TextUnformatted(content.c_str());

	if (objectInspector)
		{
			ImGui::Separator();
			ImGui::Text("Currently inspected object transformed list :");

			ThreeDObject* obj = objectInspector->getInspectedObject();

			// ----- case when no object is inspected, display & rewind scene events
			if (!obj)
			{
				ImGui::Text("Scene events:");

				ThreeDScene_DNA* scenedna = scene->getSceneDNA();
				auto& objList = scene->getObjectsRef();
				const auto& shist = scenedna->getHistory();


				if (shist.empty())
				{
					ImGui::TextDisabled("No scene events yet.");
				}
					else
					{
						const auto& shist = scenedna->getHistory();

						for (size_t i = 0; i < shist.size(); ++i)
						{
							const auto& ev = shist[i];

							std::string eventKindStr;
							if (ev.kind == SceneEventKind::AddObject)
								eventKindStr = "Add";
							else if (ev.kind == SceneEventKind::RemoveObject)
								eventKindStr = "Remove";
							else if (ev.kind == SceneEventKind::InitSnapshot)
								eventKindStr = "InitSnapshot";
							else if (ev.kind == SceneEventKind::SlotChange)
								eventKindStr = "SlotChange";
							else if (ev.kind == SceneEventKind::TransformChange)
								eventKindStr = "Transform";

							std::string line = "#" + std::to_string(i) + "  ";

							switch (ev.kind)
							{
								case SceneEventKind::InitSnapshot:
									line += " InitSnapshot";
									break;

								case SceneEventKind::AddObject:
									line += "Add Object  ";
									line += ev.objectName + "  ID=" + std::to_string(ev.objectID);
									break;

								case SceneEventKind::RemoveObject:
									line += "Remove Object  ";
									line += ev.objectName + "  ID=" + std::to_string(ev.objectID);
									break;

								case SceneEventKind::SlotChange:
									line += "Slot Change  ";
									line += ev.objectName + "  ";
									line += "from slot " + std::to_string(ev.oldSlots) + " → to slot " + std::to_string(ev.newSlots);
									break;

								case SceneEventKind::TransformChange:
									line += "Transform Change  ";
									line += ev.objectName + "  ";
									
									glm::vec3 oldScale, oldTranslation, oldSkew;
									glm::vec4 oldPerspective;
									glm::quat oldRotQ;
									
									glm::vec3 newScale, newTranslation, newSkew;
									glm::vec4 newPerspective;
									glm::quat newRotQ;
									
									if (glm::decompose(ev.oldTransform, oldScale, oldRotQ, oldTranslation, oldSkew, oldPerspective) &&
										glm::decompose(ev.newTransform, newScale, newRotQ, newTranslation, newSkew, newPerspective))
									{
										glm::vec3 oldEulerDeg = glm::degrees(glm::eulerAngles(oldRotQ));
										glm::vec3 newEulerDeg = glm::degrees(glm::eulerAngles(newRotQ));
										
										line += "T(" + std::to_string(oldTranslation.x) + "," + std::to_string(oldTranslation.y) + "," + std::to_string(oldTranslation.z) + ")";
										line += " → T(" + std::to_string(newTranslation.x) + "," + std::to_string(newTranslation.y) + "," + std::to_string(newTranslation.z) + ")";
									}
									else
									{
										line += "Matrix Transform";
									}
									break;

								default:
									line += "Unknown Event";
									break;
							}

							line += "  [tick=" + std::to_string(ev.tick) + "]";

							if (ImGui::Selectable(line.c_str(), false))
							{
								if (ev.kind == SceneEventKind::TransformChange)
								{
									scenedna->cancelTransformByID(ev.transformID);
								}
								else
								{
									for (size_t j = shist.size(); j-- > i + 1;)
									{
										const auto& futureEvent = shist[j];

										if (futureEvent.kind == SceneEventKind::AddObject)
										{
											scenedna->cancelLastAddObject(j);
										}
										else if (futureEvent.kind == SceneEventKind::RemoveObject)
										{
											scenedna->cancelLastRemoveObject(j);
										}
										else if (futureEvent.kind == SceneEventKind::SlotChange)
										{
											scenedna->cancelLastSlotChange(j);
										}
										else if (futureEvent.kind == SceneEventKind::TransformChange)
										{
											scenedna->cancelTransformByID(futureEvent.transformID);
										}
										else
										{
											scenedna->rewindToSceneEvent(j - 1);
										}
									}

									scenedna->rewindToSceneEvent(i);
								}
								
								ImGui::End();
								ImGui::PopStyleColor(6);
								return;
							}
						}
					}

			}

			else if (auto* mesh = dynamic_cast<Mesh*>(obj))
			{
				if (auto* dna = mesh->getMeshDNA())
				{
					const auto& hist = dna->getHistory();
					if (hist.empty())
					{
						ImGui::TextDisabled("No transforms recorded yet.");
					}
					else
					{
						for (size_t i = 0; i < hist.size(); ++i)
						{
							const auto& ev = hist[i];

							glm::vec3 scale, translation, skew;
							glm::vec4 perspective;
							glm::quat rotQ;

							if (!glm::decompose(ev.delta, scale, rotQ, translation, skew, perspective))
							{
								translation = glm::vec3(ev.delta[3]);
								scale = glm::vec3(1.0f);
								rotQ = glm::quat(1, 0, 0, 0);
							}
							glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(rotQ));

							std::string line = "#" + std::to_string(i) + "  " + ev.tag + "  ";

							if (ev.tag == "translate")
							{
								line += "(dx=" + std::to_string(translation.x) + 
								", dy=" + std::to_string(translation.y) +
								", dz=" + std::to_string(translation.z) + ")";
							}
							else if (ev.tag == "rotate")
							{
								line += "(rx=" + std::to_string(eulerDeg.x) + "°, " +
								"ry=" + std::to_string(eulerDeg.y) + "°, " +
								"rz=" + std::to_string(eulerDeg.z) + "°)";
							}
							else if (ev.tag == "scale")
							{
								line += "(sx=" + std::to_string(scale.x) +
								", sy=" + std::to_string(scale.y) +
								", sz=" + std::to_string(scale.z) + ")";
							}
							else if (ev.tag == "edge_modify" || ev.kind == ComponentEditKind::Edge)
							{

								line = "#" + std::to_string(i) + "  Modify Edge  ";
								line += "(verts=" + std::to_string(ev.affectedVertices.size()) + ")";
							}
							else if (ev.tag == "vertex_modify" || ev.kind == ComponentEditKind::Vertice)
							{
								line = "#" + std::to_string(i) + "  Modify Vertices  ";
								line += "(verts=" + std::to_string(ev.affectedVertices.size()) + ")";
							}
							else if (ev.tag == "face_modify" || ev.kind == ComponentEditKind::Face)
							{
								line = "#" + std::to_string(i) + "  Modify Face(s)  ";
								line += "(verts=" + std::to_string(ev.affectedVertices.size()) + ")";
							}
							else if (ev.tag == "extrude_face" || ev.kind == ComponentEditKind::Extrude)
							{
								line = "#" + std::to_string(i) + "  Extrude Face";
								line += " (dist=" + std::to_string(ev.extrude.distance) + ")";
							}
							else
							{								
								const glm::vec3 t(ev.delta[3]);
								line += "(dx=" + std::to_string(t.x) +
								", dy=" + std::to_string(t.y) +
								", dz=" + std::to_string(t.z) + ")";
							}

						line += "  tick=" + std::to_string(ev.tick);

						if (ImGui::Selectable(line.c_str(), false))
						{
							glm::mat4 target = dna->accumulatedUpTo(i + 1);
							glm::mat4 current = dna->accumulated();
							glm::mat4 delta = target * glm::inverse(current);
							std::list<ThreeDObject*> one{ obj };

							ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
							if (ev.tag == "rotate") op = ImGuizmo::ROTATE;
							else if (ev.tag == "scale") op = ImGuizmo::SCALE;

							MeshTransform::applyGizmoTransformation(scene, delta, one, op);
							
							dna->rewindEdgeHistory(i, mesh);
							dna->rewindVerticeHistory(i, mesh);
							dna->rewindFaceHistory(i, mesh);
							dna->rewindExtrudeHistory(i, mesh);
							dna->rewindToAndApply(i, mesh);

							ImGui::End();
							ImGui::PopStyleColor(6);
							return;							
						}
					}
				}
			}
		}

		ImGui::End();
		ImGui::PopStyleColor(6);
   
	}
	//---------//
}