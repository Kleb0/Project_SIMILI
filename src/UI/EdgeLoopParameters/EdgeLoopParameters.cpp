#include "UI/EdgeLoopParameters/EdgeLoopParameters.hpp"
#include "Engine/ThreeDScene.hpp"
#include <imgui_internal.h>
#include <iostream>

void EdgeLoopParameters::setScene(ThreeDScene* s) {
    scene = s;
}



void EdgeLoopParameters::render() {
    ImGui::Text("Edge Loop Parameters");
    ImGui::Separator();
    ImGui::Text("(Ajustez les paramètres ici)");

    if (ImGui::Button("Valider")) 
    {

        std::cout << "Edge Loop Parameters validated." << std::endl;       

    }
    ImGui::SameLine();

}
