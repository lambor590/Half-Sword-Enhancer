#pragma once

#include <algorithm>
#include <atomic>
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
#include "Menu/SectionStyle.h"
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

    template <typename Number>
    void AppendSpawnParams(
        std::vector<KeybindParam>& params, Number& distanceForward, Number& distanceUp, Number& scale,
        bool& snapToGround, const SpawnParamConfig& config
    ) {
        params.emplace_back(
            "snap_to_ground", "Place on Ground", &snapToGround, "Place the spawned character or item on the ground"
        );
        params.emplace_back(
            "distance_forward", config.forwardLabel, &distanceForward, config.forwardMin, config.forwardMax,
            config.forwardTooltip
        );
        params.emplace_back("distance_up", config.upLabel, &distanceUp, config.upMin, config.upMax, config.upTooltip);
        params.emplace_back("scale", "Size", &scale, config.scaleMin, config.scaleMax, config.scaleTooltip);
    }

    inline std::string EncodeData(std::string_view value) {
        static constexpr char HEX[] = "0123456789abcdef";
        std::string encoded(value.size() * 2, '\0');
        for (std::size_t index = 0; index < value.size(); ++index) {
            const auto byte = static_cast<unsigned char>(value[index]);
            encoded[index * 2] = HEX[byte >> 4U];
            encoded[index * 2 + 1] = HEX[byte & 0x0FU];
        }
        return encoded;
    }

    inline bool DecodeData(std::string_view encoded, std::string& value) {
        const auto digit = [](char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        if (encoded.size() % 2 != 0) return false;
        value.resize(encoded.size() / 2);
        for (std::size_t index = 0; index < value.size(); ++index) {
            const int high = digit(encoded[index * 2]);
            const int low = digit(encoded[index * 2 + 1]);
            if (high < 0 || low < 0) return false;
            value[index] = static_cast<char>((high << 4U) | low);
        }
        return true;
    }

    template <class Binding, class Adapter>
    void PersistBinding(Binding& binding, const BindingConfig& config, const Adapter& adapter) {
        (void)adapter.Refresh(binding);
        binding.keybind.tooltip = binding.summary;
        binding.spawnSnapshot.store(adapter.MakeSnapshot(binding), std::memory_order_release);

        auto& configManager = ConfigManager::Get();
        const auto section = BindingSection(config, binding.id);
        configManager.BatchSave([&] {
            configManager.SetString(section.c_str(), "name", binding.name);
            adapter.SaveFields(binding, configManager, section.c_str());
        });
    }

    template <class Binding, class Adapter> class BindingList {
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
                std::snprintf(
                    binding->name, sizeof(binding->name), "%s",
                    configManager.GetString(section.c_str(), "name", config.defaultName).c_str()
                );
                adapter.LoadFields(*binding, configManager, section.c_str());
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
                    configManager.SetString(section.c_str(), "name", binding.name);
                    adapter.SaveFields(binding, configManager, section.c_str());
                }
            });
        }

        void AddFromCurrentSelection() {
            auto binding = std::make_shared<Binding>();
            binding->id = nextBindingId++;
            if (!adapter.Capture(*binding)) return;
            std::snprintf(binding->name, sizeof(binding->name), "%s", binding->summary.c_str());
            const auto section = BindingSection(config, binding->id);
            bindings.push_back(binding);
            Save();
            InitKeybind(binding, section);
        }

        void Render() {
            if (GuiUtils::Button("Add Spawn Shortcut")) AddFromCurrentSelection();
            GuiUtils::HelpTooltip(config.addTooltip);

            if (bindings.empty()) {
                ImGui::TextColored(DefaultStyle::PARCHMENT_DARK, "%s", config.emptyText);
                return;
            }

            for (auto& bindingPtr : bindings) {
                auto& binding = *bindingPtr;
                ImGui::PushID(binding.id);
                ImGui::Separator();

                const auto& style = ImGui::GetStyle();
                const float inputWidth =
                    GuiUtils::ResolveControlWidth({SectionStyle::FIELD_MIN_WIDTH, 220.0f, 220.0f}, 220.0f);
                const float actionsWidth = GuiUtils::ButtonNaturalWidth("Use Current Setup") + style.ItemSpacing.x +
                                           GuiUtils::ButtonNaturalWidth("Delete");
                const bool actionsFit =
                    inputWidth + style.ItemSpacing.x + actionsWidth <= ImGui::GetContentRegionAvail().x;
                ImGui::SetNextItemWidth(inputWidth);
                if (ImGui::InputText("##BindingName", binding.name, sizeof(binding.name))) {
                    binding.keybind.name = binding.name;
                    KeybindManager::UpdateBindingName(&binding.key, binding.name);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) SaveName(binding);
                if (actionsFit) ImGui::SameLine();
                if (GuiUtils::Button("Use Current Setup")) {
                    if (adapter.Capture(binding)) {
                        binding.keybind.tooltip = binding.summary;
                        PersistBinding(binding, config, adapter);
                    }
                }
                GuiUtils::HelpTooltip(config.updateTooltip);
                (void)GuiUtils::SameLineIfFitsButton("Delete");
                if (GuiUtils::Button("Delete")) pendingDeleteBindingId = binding.id;

                ImGui::PushStyleColor(ImGuiCol_Text, DefaultStyle::PARCHMENT_DARK);
                ImGui::TextWrapped("%s", binding.summary.c_str());
                ImGui::PopStyleColor();
                binding.keybind.Render();
                ImGui::PopID();
            }

            RenderDeletePopup();
        }

        void PublishSnapshots() const {
            for (auto& binding : bindings)
                PublishSpawnSnapshot(*binding);
        }

    private:
        void SaveName(const Binding& binding) const {
            auto& configManager = ConfigManager::Get();
            const auto section = BindingSection(config, binding.id);
            configManager.SetString(section.c_str(), "name", binding.name);
            configManager.SaveConfig();
        }

        void PublishSpawnSnapshot(Binding& binding) const {
            adapter.Refresh(binding);
            binding.keybind.tooltip = binding.summary;
            binding.spawnSnapshot.store(adapter.MakeSnapshot(binding), std::memory_order_release);
        }

        void InitKeybind(const std::shared_ptr<Binding>& binding, const std::string& section) {
            std::weak_ptr<Binding> weakBinding = binding;
            std::vector<KeybindParam> params;
            adapter.AppendParams(*binding, params, config.spawnParams);

            KeybindEntry definition{
                .name = binding->name,
                .tooltip = binding->summary,
                .configSection = section,
                .keyPtr = &binding->key,
                .callback =
                    [adapter = adapter, weakBinding]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
                        const auto bindingLifetime = weakBinding.lock();
                        if (!bindingLifetime) return;
                        const auto snapshot = bindingLifetime->spawnSnapshot.load(std::memory_order_acquire);
                        if (snapshot) adapter.Spawn(*snapshot, runtime);
                    },
                .persistParams = false,
                .params = std::move(params),
                .onParamsChanged =
                    [bindingConfig = config, bindingAdapter = adapter, weakBinding]() {
                        if (const auto bindingLifetime = weakBinding.lock())
                            PersistBinding(*bindingLifetime, bindingConfig, bindingAdapter);
                    },
            };
            binding->keybind.AdoptDefinition(definition);
            binding->keybind.Init();
            PublishSpawnSnapshot(*binding);
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
                        const auto section = BindingSection(config, (*it)->id);
                        ConfigManager::Get().DeleteSection(section.c_str());
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
