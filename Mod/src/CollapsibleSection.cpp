#include <iostream>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Gui.h"

void CollapsibleSection::Render() {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    bool isOpen = ImGui::CollapsingHeader(name.c_str());
    
    if (isOpen) [[likely]] {
        SectionStyle::StyleRAII style;
        
        const size_t count = functions.size();
        for (size_t i = 0; i < count; ++i) {
            functions[i]->Render();
            if (i < count - 1) ImGui::Spacing();
        }
    }
}