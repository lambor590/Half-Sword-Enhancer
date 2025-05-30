#include <iostream>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Gui.h"

void CollapsibleSection::Render() {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    bool isOpen = ImGui::CollapsingHeader(name.c_str());
    
    if (isOpen) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 25.0f);
        
        ImGui::Indent(10.0f);
        ImGui::Spacing();
        
        const size_t count = functions.size();
        for (size_t i = 0; i < count; ++i) {
            functions[i]->Render();
            if (i < count - 1) ImGui::Spacing();
        }
        
        ImGui::Unindent(10.0f);
        ImGui::PopStyleVar(3);
    }
}

void CollapsibleSection::AddFunction(std::unique_ptr<IMenuFunction> function) {
    functions.push_back(std::move(function));
}