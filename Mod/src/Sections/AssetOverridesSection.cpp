#include "Menu/Sections/Settings/AssetOverridesSection.h"

#include "Menu/SectionStyle.h"
#include "Utils/AssetOverrideManager.h"
#include "Utils/PresetUtils.h"
#include "imgui/imgui.h"

AssetOverridesSection::AssetOverridesSection(ModContext& ctx) : Section(ctx, SECTION) {}

void AssetOverridesSection::OnOpen() {
    AssetOverrideManager::Get().RequestRefresh();
}

void AssetOverridesSection::Render() {
    const SectionStyle::StyleRAII style;

    auto& manager = AssetOverrideManager::Get();
    const auto stats = manager.GetStats();

    if (ImGui::Button("Refresh")) {
        manager.RequestRefresh();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Folder")) {
        PresetUtils::OpenInExplorer(manager.GetRootPath());
    }

    ImGui::Spacing();
    ImGui::Text("Files: %d", stats.files);
    ImGui::Text("Loaded Textures: %d", stats.loaded);
    ImGui::Text("Updated Materials: %d", stats.appliedMaterials);
    ImGui::Text("Scanned Components: %d", stats.scannedComponents);
    ImGui::Text("Scanned Materials: %d", stats.scannedMaterials);
    ImGui::Text("Unmatched Textures: %d", stats.unmatched);
    ImGui::Text("Errors: %d", stats.errors);
}
