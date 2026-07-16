#include "Menu/Sections/Settings/AssetOverridesSection.h"

#include "Utils/AssetOverrideManager.h"
#include "Utils/GuiUtils.h"
#include "Utils/PresetUtils.h"
#include "imgui/imgui.h"

AssetOverridesSection::AssetOverridesSection(ModContext& ctx) : Section(ctx, SECTION) {}

void AssetOverridesSection::OnOpen() {
    AssetOverrideManager::Get().RequestRefresh();
}

void AssetOverridesSection::Render() {
    auto& manager = AssetOverrideManager::Get();
    const auto stats = manager.GetStats();

    ImGui::SeparatorText("Custom Textures");
    ImGui::TextWrapped("Add replacement images to the texture folder, then refresh them here.");
    ImGui::Spacing();

    if (GuiUtils::Button("Refresh Textures", GuiUtils::ButtonTone::Primary)) manager.RequestRefresh();
    (void)GuiUtils::SameLineIfFitsButton("Open Texture Folder");
    if (GuiUtils::Button("Open Texture Folder")) PresetUtils::OpenInExplorer(manager.GetRootPath());

    if (stats.files == 0) {
        GuiUtils::RenderCallout(
            "override-empty", "No custom textures found. Open the folder to add some.", GuiUtils::CalloutTone::Info
        );
    } else if (stats.errors > 0) {
        GuiUtils::RenderCallout(
            "override-errors", "Some custom textures could not be used. Check their image format and refresh again.",
            GuiUtils::CalloutTone::Warning
        );
    } else if (stats.unmatched > 0) {
        GuiUtils::RenderCallout(
            "override-unmatched", "Some custom textures do not match anything visible in the current map.",
            GuiUtils::CalloutTone::Info
        );
    }

    ImGui::SeparatorText("Status");
    constexpr ImGuiTableFlags STATS_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##OverrideStats", 2, STATS_FLAGS)) {
        ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("000000").x);
        ImGui::TableHeadersRow();
        const auto row = [](const char* label, int value) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::Text("%d", value);
        };
        row("Files found", stats.files);
        row("Textures in use", stats.loaded);
        row("Surfaces changed", stats.appliedMaterials);
        row("Unused textures", stats.unmatched);
        row("Files needing attention", stats.errors);
        ImGui::EndTable();
    }
}
