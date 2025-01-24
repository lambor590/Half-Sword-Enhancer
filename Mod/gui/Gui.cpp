#include "Gui.h"

Gui* Gui::s_instance = nullptr;

Logger logger("Gui");

void Gui::Setup() {
    IMGUI_CHECKVERSION();
    
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = "HS-Enhancer-imgui.ini";
    
    ImGui::StyleColorsDark();
    
    s_instance = this;
}

void Gui::Render() {
    ImGui::NewFrame();

    ImGui::Begin("Half Sword Enhancer", nullptr,
        ImGuiWindowFlags_MenuBar | 
        ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Settings", nullptr);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::Text("Press INSERT to hide/show this window");

    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}