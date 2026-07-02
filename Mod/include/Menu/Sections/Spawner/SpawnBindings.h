#pragma once

#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigManager.h"
#include "DefaultStyle.h"
#include "KeybindManager.h"
#include "Menu/Keybind.h"
#include "Menu/SectionConfig.h"
#include "Utils/GuiUtils.h"

namespace SpawnBindings {
    struct SpawnParamConfig {
        const char* forwardLabel;
        float forwardMin;
        float forwardMax;
        const char* forwardTooltip;
        const char* upLabel;
        float upMin;
        float upMax;
        const char* upTooltip;
        float scaleMin;
        float scaleMax;
        const char* scaleTooltip;
    };

    struct BindingConfig {
        const char* indexSection;
        const char* bindingPrefix;
        const char* defaultName;
        const char* addTooltip;
        const char* emptyText;
        const char* updateTooltip;
        const char* deletePopupTitle;
        const char* deletePrompt;
        SpawnParamConfig spawnParams;
    };

    inline std::string BindingSection(const BindingConfig& config, int id) {
        return std::string(config.bindingPrefix) + std::to_string(id);
    }

    inline void AppendSpawnParams(
        std::vector<KeybindParam>& params, SpawnConfig& spawn, const SpawnParamConfig& config
    ) {
        params.emplace_back(
            "snap_to_ground", "Snap to Ground", &spawn.snapToGround, "Automatically adjust height to touch the ground"
        );
        params.emplace_back(
            "distance_forward", config.forwardLabel, &spawn.distanceForward, config.forwardMin, config.forwardMax,
            config.forwardTooltip
        );
        params.emplace_back("distance_up", config.upLabel, &spawn.distanceUp, config.upMin, config.upMax, config.upTooltip);
        params.emplace_back("scale", "Scale", &spawn.scale, config.scaleMin, config.scaleMax, config.scaleTooltip);
    }

    template <class Binding>
    void LoadCommon(
        ConfigManager& config, Binding& binding, std::string_view section, const BindingConfig& bindingConfig
    ) {
        binding.key = config.GetInt(section, "key", -1);
        std::snprintf(
            binding.name, sizeof(binding.name), "%s",
            config.GetString(section, "name", bindingConfig.defaultName).c_str()
        );
        binding.summary = config.GetString(section, "summary", binding.name);
        binding.spawn = {
            .distanceForward = config.GetFloat(section, "distance_forward", binding.spawn.distanceForward),
            .distanceUp = config.GetFloat(section, "distance_up", binding.spawn.distanceUp),
            .scale = config.GetFloat(section, "scale", binding.spawn.scale),
            .snapToGround = config.GetBool(section, "snap_to_ground", binding.spawn.snapToGround),
        };
    }

    template <class Binding>
    void SaveCommon(ConfigManager& config, const Binding& binding, std::string_view section) {
        config.SetString(section, "name", binding.name);
        config.SetString(section, "summary", binding.summary);
        config.SetInt(section, "key", binding.key);
        config.SetBool(section, "snap_to_ground", binding.spawn.snapToGround);
        config.SetFloat(section, "distance_forward", binding.spawn.distanceForward);
        config.SetFloat(section, "distance_up", binding.spawn.distanceUp);
        config.SetFloat(section, "scale", binding.spawn.scale);
    }

    template <class Binding, class Adapter>
    class BindingList {
    public:
        BindingList(
            std::vector<std::shared_ptr<Binding>>& bindings, int& nextBindingId, int& pendingDeleteBindingId,
            const BindingConfig& config, Adapter adapter
        )
            : bindings(bindings),
              nextBindingId(nextBindingId),
              pendingDeleteBindingId(pendingDeleteBindingId),
              config(config),
              adapter(adapter) {}

        void Load() {
            auto& configManager = ConfigManager::Get();
            nextBindingId = configManager.GetInt(config.indexSection, "next_id", 1);
            const int count = (std::min)(configManager.GetInt(config.indexSection, "count", 0), 64);

            for (int i = 0; i < count; ++i) {
                char idKey[16];
                std::snprintf(idKey, sizeof(idKey), "id_%d", i);
                const int id = configManager.GetInt(config.indexSection, idKey, 0);
                if (id <= 0) continue;

                auto binding = std::make_shared<Binding>();
                binding->id = id;
                const auto section = BindingSection(config, id);
                LoadCommon(configManager, *binding, section, config);
                adapter.LoadFields(*binding, configManager, section);
                InitKeybind(binding, section);
                nextBindingId = (std::max)(nextBindingId, id + 1);
                bindings.push_back(std::move(binding));
            }
        }

        void Save() {
            auto& configManager = ConfigManager::Get();
            configManager.BatchSave([&] {
                configManager.DeleteSection(config.indexSection);
                configManager.SetInt(config.indexSection, "next_id", nextBindingId);
                configManager.SetInt(config.indexSection, "count", static_cast<int>(bindings.size()));

                for (size_t i = 0; i < bindings.size(); ++i) {
                    const auto& binding = *bindings[i];
                    char idKey[16];
                    std::snprintf(idKey, sizeof(idKey), "id_%zu", i);
                    configManager.SetInt(config.indexSection, idKey, binding.id);

                    const auto section = BindingSection(config, binding.id);
                    SaveCommon(configManager, binding, section);
                    adapter.SaveFields(binding, configManager, section);
                }
            });
        }

        void AddFromCurrentSelection() {
            auto binding = std::make_shared<Binding>();
            binding->id = nextBindingId++;
            if (!adapter.Capture(*binding)) return;
            std::snprintf(binding->name, sizeof(binding->name), "%s", binding->summary.c_str());
            InitKeybind(binding, BindingSection(config, binding->id));
            bindings.push_back(std::move(binding));
            Save();
        }

        void Render() {
            if (ImGui::Button("Add Spawn Binding")) AddFromCurrentSelection();
            TooltipHelper::ShowTooltip(config.addTooltip);

            if (bindings.empty()) {
                ImGui::TextColored(DefaultStyle::PARCHMENT_DARK, "%s", config.emptyText);
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
                    Save();
                }
                ImGui::SameLine();
                if (ImGui::Button("Update")) {
                    (void)adapter.Capture(binding);
                    binding.keybind.tooltip = binding.summary;
                    Save();
                }
                TooltipHelper::ShowTooltip(config.updateTooltip);
                ImGui::SameLine();
                if (ImGui::Button("Delete")) pendingDeleteBindingId = binding.id;

                ImGui::TextColored(DefaultStyle::PARCHMENT_DARK, "%s", binding.summary.c_str());
                binding.keybind.Render();
                ImGui::PopID();
            }

            RenderDeletePopup();
        }

    private:
        void InitKeybind(const std::shared_ptr<Binding>& binding, std::string_view section) {
            std::weak_ptr<Binding> weakBinding = binding;
            std::vector<KeybindParam> params;
            params.reserve(4 + Adapter::EXTRA_PARAM_COUNT);
            adapter.AppendLeadingParams(*binding, params);
            AppendSpawnParams(params, binding->spawn, config.spawnParams);
            adapter.AppendTrailingParams(*binding, params);

            binding->keybind = {
                .name = binding->name,
                .tooltip = binding->summary,
                .configSection = std::string(section),
                .keyPtr = &binding->key,
                .callback =
                    [adapter = adapter, weakBinding]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                        if (auto binding = weakBinding.lock()) adapter.Spawn(*binding, runtime);
                    },
                .params = std::move(params),
            };
            binding->keybind.Init();
        }

        void RenderDeletePopup() {
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0, 0, 0, 0.6f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, GuiUtils::K_POPUP_PADDING);
            if (pendingDeleteBindingId != -1) ImGui::OpenPopup(config.deletePopupTitle);
            if (ImGui::BeginPopupModal(
                    config.deletePopupTitle, nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings
                )) {
                ImGui::Text("%s", config.deletePrompt);
                ImGui::Spacing();
                if (ImGui::Button("Delete")) {
                    auto it = std::find_if(bindings.begin(), bindings.end(), [this](const auto& binding) {
                        return binding->id == pendingDeleteBindingId;
                    });
                    if (it != bindings.end()) {
                        KeybindManager::UnregisterKeybind((*it)->keybind.keyPtr);
                        ConfigManager::Get().DeleteSection(BindingSection(config, (*it)->id));
                        bindings.erase(it);
                        Save();
                    }
                    pendingDeleteBindingId = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    pendingDeleteBindingId = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        std::vector<std::shared_ptr<Binding>>& bindings;
        int& nextBindingId;
        int& pendingDeleteBindingId;
        const BindingConfig& config;
        Adapter adapter;
    };
}
