#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "ConfigManager.h"
#include "KeybindManager.h"
#include "Menu/IMenuFunction.h"

class KeybindsSection : public CollapsibleSection {
private:
    bool waitingForToggleKey = false;
    bool waitingForUnbindKey = false;

public:
    KeybindsSection() : CollapsibleSection("Keybinds") {
        KeybindManager::Initialize();
    }

    void Render() override {
        if (ImGui::CollapsingHeader("Keybinds")) {
            bool changed = false;
            
            changed |= RenderKeybind(
                waitingForToggleKey, 
                KeybindManager::GetToggleGuiKey(),
                "Toggle GUI Key",
                "Change key to show/hide interface"
            );
            
            changed |= RenderKeybind(
                waitingForUnbindKey, 
                KeybindManager::GetUnbindKey(),
                "Unbind Key",
                "Change key to unbind shortcuts"
            );
            
            if (changed)
                KeybindManager::SaveKeybinds();
        }
    }

private:
    bool RenderKeybind(bool& waitingForKey, int& key, const char* label, const char* tooltip) {
        const char* keyName = waitingForKey ? "Press any key..." : KeybindManager::GetKeyName(key);
        
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