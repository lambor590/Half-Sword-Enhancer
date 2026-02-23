#pragma once

#include "Menu/ICollapsibleSection.h"
#include "Utils/MapRegistry.h"
#include "Utils/GuiUtils.h"

class MapLoaderSection : public CollapsibleSection {
    int selectedMapIndex = 0;
    float cachedComboW = 0.0f;
    size_t cachedMapCount = 0;
    std::string cachedLevelName;
    bool levelNameDirty = true;

    static constexpr ImVec4 kYellowText{1.0f, 0.85f, 0.3f, 1.0f};
    static constexpr ImVec4 kOrangeText{1.0f, 0.5f, 0.3f, 1.0f};
    static constexpr ImVec4 kGrayText{0.5f, 0.5f, 0.5f, 1.0f};

    static const char* MapNameGetter(void* data, int idx) {
        return static_cast<const std::vector<MapEntry>*>(data)->operator[](idx).displayName.c_str();
    }

    void RefreshLevelName() {
        if (!levelNameDirty) return;
        if (ComponentValidator::Validate(world)) {
            SDK::FString currentLevel = SDK::UGameplayStatics::GetCurrentLevelName(world, true);
            cachedLevelName = currentLevel.ToString();
        } else {
            cachedLevelName.clear();
        }
        levelNameDirty = false;
    }

public:
    MapLoaderSection() : CollapsibleSection("Map Loader") {}

    void RenderContent() override {
        const SectionStyle::StyleRAII style;

        auto& reg = MapRegistry::Get();
        auto scanState = reg.GetState();

        if (scanState == ScanState::NotStarted || scanState == ScanState::Scanning) {
            if (scanState == ScanState::NotStarted) reg.RequestScan();
            ImGui::TextColored(kYellowText, "Scanning maps...");
            return;
        }

        if (scanState == ScanState::Failed) {
            ImGui::TextColored(kOrangeText, "No maps found");
            if (ImGui::Button("Retry")) reg.RequestRescan();
            return;
        }

        const auto& maps = reg.GetMaps();

        RefreshLevelName();
        if (!cachedLevelName.empty()) {
            ImGui::Text("Current: %s", cachedLevelName.c_str());
            ImGui::Spacing();
        }

        if (selectedMapIndex >= static_cast<int>(maps.size()))
            selectedMapIndex = 0;

        ImGui::Text("Map");
        if (cachedMapCount != maps.size()) {
            cachedComboW = GuiUtils::CalcComboWidth(MapNameGetter, const_cast<void*>(static_cast<const void*>(&maps)), static_cast<int>(maps.size()));
            cachedMapCount = maps.size();
        }
        ImGui::SetNextItemWidth(cachedComboW);
        ImGui::Combo("##MapSelector", &selectedMapIndex, MapNameGetter, const_cast<void*>(static_cast<const void*>(&maps)), static_cast<int>(maps.size()));

        ImGui::Spacing();
        if (ImGui::Button("Load Map")) {
            if (ComponentValidator::Validate(world) && selectedMapIndex < static_cast<int>(maps.size())) {
                std::string packageName = maps[selectedMapIndex].packageName;
                GameHook::QueueAction([this, pn = std::move(packageName)]() {
                    std::wstring wideName(pn.begin(), pn.end());
                    auto levelName = SDK::BasicFilesImpleUtils::StringToName(wideName.c_str());
                    SDK::UGameplayStatics::OpenLevel(
                        world, levelName,
                        true, SDK::FString(L""));
                    levelNameDirty = true;
                });
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Rescan Maps")) {
            reg.RequestRescan();
            selectedMapIndex = 0;
        }
        ImGui::SameLine();
        ImGui::TextColored(kGrayText, "(%zu maps)", maps.size());
    }
};
