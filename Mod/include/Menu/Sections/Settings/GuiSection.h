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

class GuiSection : public CollapsibleSection {
private:
    bool waitingForToggleKey = false;
    bool waitingForUnbindKey = false;

public:
    GuiSection() : CollapsibleSection("GUI") {}

    void Render() override {
        bool isOpen = ImGui::CollapsingHeader(name.c_str());
        
        if (isOpen) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 25.0f);
            
            ImGui::Indent(10.0f);
            ImGui::Spacing();

            bool changed = false;
            
            changed |= RenderKeybind(
                waitingForToggleKey, 
                KeybindManager::GetToggleGuiKey(),
                "Toggle GUI Key",
                "Change key to show/hide interface"
            );
            
            ImGui::Spacing();
            
            changed |= RenderKeybind(
                waitingForUnbindKey, 
                KeybindManager::GetUnbindKey(),
                "Unbind Key",
                "Change key to unbind shortcuts"
            );
            
            if (changed)
                KeybindManager::SaveKeybinds();
                
            ImGui::Unindent(10.0f);
            ImGui::PopStyleVar(3);
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