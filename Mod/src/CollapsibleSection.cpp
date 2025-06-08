#include <iostream>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"
#include "Gui.h"

void CollapsibleSection::Render() {
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    const bool isOpen = ImGui::CollapsingHeader(name.c_str());
    
    if (isOpen) [[likely]] {
        const SectionStyle::StyleRAII style;
        
        const size_t count = functions.size();
        for (size_t i = 0; i < count; ++i) {
            functions[i]->Render();
            if (i + 1 < count) {
                ImGui::Spacing();
            }
        }
    }
}