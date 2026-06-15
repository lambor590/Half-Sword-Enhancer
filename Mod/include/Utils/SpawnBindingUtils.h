#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ConfigManager.h"
#include "DefaultStyle.h"
#include "KeybindManager.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"
#include "Utils/GuiUtils.h"
#include "imgui/imgui.h"

namespace SpawnBindingUtils {

    template <size_t N> void CopyName(char (&dst)[N], std::string_view src) {
        const size_t len = min(src.size(), N - 1);
        std::memcpy(dst, src.data(), len);
        dst[len] = '\0';
    }

    inline SpawnConfig LoadSpawnConfig(std::string_view section, const SpawnConfig& fallback) {
        auto& config = ConfigManager::Get();
        return {
            .distanceForward = config.GetFloat(section, "distance_forward", fallback.distanceForward),
            .distanceUp = config.GetFloat(section, "distance_up", fallback.distanceUp),
            .scale = config.GetFloat(section, "scale", fallback.scale),
            .snapToGround = config.GetBool(section, "snap_to_ground", fallback.snapToGround),
        };
    }

    inline void SaveSpawnConfig(std::string_view section, const SpawnConfig& spawn) {
        auto& config = ConfigManager::Get();
        config.SetBool(section, "snap_to_ground", spawn.snapToGround);
        config.SetFloat(section, "distance_forward", spawn.distanceForward);
        config.SetFloat(section, "distance_up", spawn.distanceUp);
        config.SetFloat(section, "scale", spawn.scale);
    }

    template <typename Binding, typename SectionFn, typename LoadExtraFn, typename InitFn>
    void LoadBindings(
        std::string_view rootSection, int& nextId, std::vector<std::shared_ptr<Binding>>& bindings,
        SectionFn sectionName, std::string_view defaultName, LoadExtraFn loadExtra, InitFn init
    ) {
        auto& config = ConfigManager::Get();
        nextId = config.GetInt(rootSection, "next_id", 1);
        const int count = min(config.GetInt(rootSection, "count", 0), 64);

        for (int i = 0; i < count; ++i) {
            char idKey[16];
            std::snprintf(idKey, sizeof(idKey), "id_%d", i);
            int id = config.GetInt(rootSection, idKey, 0);
            if (id <= 0) continue;

            auto binding = std::make_shared<Binding>();
            binding->id = id;
            auto section = sectionName(id);
            binding->key = config.GetInt(section, "key", -1);
            CopyName(binding->name, config.GetString(section, "name", defaultName));
            binding->summary = config.GetString(section, "summary", binding->name);

            loadExtra(*binding, section, config);
            init(binding);
            nextId = max(nextId, id + 1);
            bindings.push_back(std::move(binding));
        }
    }

    template <typename Binding, typename SectionFn, typename SaveExtraFn>
    void SaveBindings(
        std::string_view rootSection, int nextId, const std::vector<std::shared_ptr<Binding>>& bindings,
        SectionFn sectionName, SaveExtraFn saveExtra
    ) {
        auto& config = ConfigManager::Get();
        config.BatchSave([&] {
            config.DeleteSection(rootSection);
            config.SetInt(rootSection, "next_id", nextId);
            config.SetInt(rootSection, "count", static_cast<int>(bindings.size()));

            for (size_t i = 0; i < bindings.size(); ++i) {
                auto& binding = *bindings[i];
                char idKey[16];
                std::snprintf(idKey, sizeof(idKey), "id_%zu", i);
                config.SetInt(rootSection, idKey, binding.id);

                auto section = sectionName(binding.id);
                config.SetString(section, "name", binding.name);
                config.SetString(section, "summary", binding.summary);
                config.SetInt(section, "key", binding.key);
                saveExtra(binding, section, config);
            }
        });
    }

    template <typename Binding, typename Callback, typename Params>
    void InitKeybind(
        const std::shared_ptr<Binding>& binding, std::string configSection, Callback&& callback, Params&& params
    ) {
        std::weak_ptr<Binding> weakBinding = binding;
        auto storedCallback = std::make_shared<std::decay_t<Callback>>(std::forward<Callback>(callback));
        auto run = [weakBinding, storedCallback]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
            if (auto binding = weakBinding.lock()) (*storedCallback)(*binding, runtime);
        };
        binding->keybind = {
            .name = binding->name,
            .tooltip = binding->summary,
            .configSection = std::move(configSection),
            .keyPtr = &binding->key,
            .callback = std::move(run),
            .params = std::forward<Params>(params),
        };
        binding->keybind.Init();
    }

    template <typename Binding, typename AddFn, typename UpdateFn, typename SectionFn, typename SaveFn>
    void RenderList(
        std::vector<std::shared_ptr<Binding>>& bindings, int& pendingDeleteId, const char* addTooltip,
        const char* emptyText, const char* updateTooltip, const char* popupTitle, const char* popupText, AddFn add,
        UpdateFn update, SectionFn sectionName, SaveFn save
    ) {
        if (ImGui::Button("Add Spawn Binding")) add();
        TooltipHelper::ShowTooltip(addTooltip);

        if (bindings.empty()) {
            ImGui::TextColored(DefaultStyle::PARCHMENT_DARK, "%s", emptyText);
            return;
        }

        for (auto& bindingPtr : bindings) {
            auto& binding = *bindingPtr;
            ImGui::PushID(binding.id);
            ImGui::Separator();

            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::InputText("##BindingName", binding.name, sizeof(binding.name))) {
                binding.keybind.name = binding.name;
                KeybindManager::UpdateBindingName(&binding.key, binding.name);
                save();
            }
            ImGui::SameLine();
            if (ImGui::Button("Update")) {
                update(binding);
                binding.keybind.tooltip = binding.summary;
                save();
            }
            TooltipHelper::ShowTooltip(updateTooltip);
            ImGui::SameLine();
            if (ImGui::Button("Delete")) pendingDeleteId = binding.id;

            ImGui::TextColored(DefaultStyle::PARCHMENT_DARK, "%s", binding.summary.c_str());
            binding.keybind.Render();

            ImGui::PopID();
        }

        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
        if (pendingDeleteId != -1) ImGui::OpenPopup(popupTitle);
        if (ImGui::BeginPopupModal(
                popupTitle, nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
            )) {
            ImGui::Text("%s", popupText);
            ImGui::Spacing();
            if (ImGui::Button("Delete")) {
                auto it = std::ranges::find_if(bindings, [pendingDeleteId](const auto& binding) {
                    return binding->id == pendingDeleteId;
                });
                if (it != bindings.end()) {
                    KeybindManager::UnregisterKeybind((*it)->keybind.keyPtr);
                    ConfigManager::Get().DeleteSection(sectionName((*it)->id));
                    bindings.erase(it);
                    save();
                }
                pendingDeleteId = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                pendingDeleteId = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

} // namespace SpawnBindingUtils
