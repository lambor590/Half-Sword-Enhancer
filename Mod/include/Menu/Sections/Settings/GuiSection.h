#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Menu/IMenuFunction.h"
#include "DefaultStyle.h"

namespace {
    constexpr const char* toggleGuiLabel = "Toggle GUI Key";
    constexpr const char* unbindLabel = "Unbind Key";
    constexpr const char* toggleTooltip = "Change key to show/hide interface";
    constexpr const char* unbindTooltip = "Change key to unbind shortcuts";
    constexpr const char* pressAnyKeyText = "Press any key...";
}

class GuiSection : public CollapsibleSection {
private:
    bool waitingForToggleKey = false;
    bool waitingForUnbindKey = false;

public:
    GuiSection() : CollapsibleSection("GUI") {}

    void Render() override {
        bool isOpen = ImGui::CollapsingHeader(name.c_str());
        
        if (isOpen) {
            SectionStyle::StyleRAII style;
            
            bool changed = false;
            
            changed |= RenderKeybind(
                waitingForToggleKey, 
                KeybindManager::GetToggleGuiKey(),
                toggleGuiLabel,
                toggleTooltip
            );
            
            ImGui::Spacing();
            
            changed |= RenderKeybind(
                waitingForUnbindKey, 
                KeybindManager::GetUnbindKey(),
                unbindLabel,
                unbindTooltip
            );
            
            if (changed)
                KeybindManager::SaveKeybinds();
        }
    }

private:
    bool RenderKeybind(bool& waitingForKey, int& key, const char* label, const char* tooltip) {
        const char* keyName = waitingForKey ? pressAnyKeyText : KeybindManager::GetKeyName(key);
        
        ImGui::AlignTextToFramePadding();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize(keyName).x + 20);
        
        if (ImGui::Button(keyName)) {
            waitingForKey = true;
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", tooltip);
            ImGui::EndTooltip();
        }
        
        ImGui::SameLine();
        ImGui::Text("%s", label);
        
        return KeybindManager::HandleKeyPress(waitingForKey, key);
    }
};