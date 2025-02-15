#include "MenuSection.h"
#include "Gui.h"

void MenuSection::Render() {
    for (auto& item : items) {
        if (item.isCollapsible) {
            if (ImGui::CollapsingHeader(item.name.c_str())) {
                ImGui::Indent();
                RenderItemContent(item);
                ImGui::Unindent();
            }
        } else {
            RenderItemContent(item);
        }
    }
}

void MenuSection::RenderItemContent(const MenuItem& item) {
    Action* action = ActionManager::Get().GetAction(item.id);
    if (!action) return;

    // Render enable/disable toggle
    ImGui::Checkbox(("##toggle" + item.id).c_str(), &action->enabled);
    
    ImGui::SameLine();
    ImGui::Text("%s", action->name.c_str());

    // Render keybind
    ImGui::SameLine();
    char keyName[32];
    sprintf_s(keyName, "[%c]##key%s", action->keyBinding ? (char)action->keyBinding : '?', item.id.c_str());
    if (ImGui::Button(keyName)) {
        if (Gui* gui = MenuManager::Get().GetGuiInstance()) {
            gui->SetWaitingForKey(true, item.id);
        }
    }

    // Render description if available
    if (!item.description.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", item.description.c_str());
        }
    }
}