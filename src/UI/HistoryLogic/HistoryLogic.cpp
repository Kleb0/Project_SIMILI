#include "UI/HistoryLogic/HistoryLogic.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp" 
#include "Engine/ThreeDInteractions/MeshTransform.hpp" 
#include <imgui.h>
#include <glm/gtc/matrix_inverse.hpp> 

HistoryLogic::HistoryLogic() {}

void HistoryLogic::setTitle(const std::string& t) { title = t; }
void HistoryLogic::setText(const std::string& t)  { content = t; }

void HistoryLogic::render()
{
    ImGui::PushStyleColor(ImGuiCol_TitleBg,       IM_COL32(0,128,0,255));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(0,200,0,255));
    ImGui::PushStyleColor(ImGuiCol_Tab,                ImVec4(0.00f,0.50f,0.00f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,         ImVec4(0.00f,0.70f,0.00f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabActive,          ImVec4(0.00f,0.80f,0.00f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0.00f,0.60f,0.00f,1.0f));

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0,0,0,255));
    ImGui::Begin(title.c_str());
    ImGui::PopStyleColor();

    ImGui::TextUnformatted(content.c_str());

    if (objectInspector)
    {
        ImGui::Separator();
        ImGui::Text("Currently inspected object transformed list :");

        ThreeDObject* obj = objectInspector->getInspectedObject();
        if (!obj)
        {
            ImGui::TextDisabled("No single object inspected (multiple selection or none).");
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
                        const glm::vec3 t(ev.delta[3]);

                        if (ImGui::Selectable(
                                (std::string("#") + std::to_string(i) +
                                 "  " + ev.tag +
                                 "  (dx=" + std::to_string(t.x) +
                                 ", dy=" + std::to_string(t.y) +
                                 ", dz=" + std::to_string(t.z) +
                                 ")  tick=" + std::to_string(ev.tick)).c_str(),
                                false))
                        {
         
                            glm::mat4 target  = dna->accumulatedUpTo(i + 1);
                            glm::mat4 current = dna->accumulated();
                            glm::mat4 delta   = target * glm::inverse(current);
                            std::list<ThreeDObject*> one{ obj };
                            MeshTransform::applyGizmoTransformation(delta, one);
                            dna->rewindTo(i);
                        }
                    }
                }
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(6);
}