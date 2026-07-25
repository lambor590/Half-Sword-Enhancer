#include "Utils/LoadoutPresetResolver.h"

#include <string>
#include <utility>

#include "ConfigManager.h"
#include "Utils/PresetLinkResolution.h"

LoadoutPresetResolver::LoadoutPresetResolver() : appDataRoot_(ConfigManager::GetAppDataPath()) {}

LoadoutPresetResolver::LoadoutPresetResolver(std::filesystem::path appDataRoot)
    : appDataRoot_(std::move(appDataRoot)) {}

PresetOperationResult LoadoutPresetData::ValidateForSave(const std::filesystem::path& appDataRoot) const {
    return PresetLinkResolution::ValidateForSave<LoadoutPresetSerializer>(*this, appDataRoot);
}

PresetResolveResult<ResolvedLoadoutPresetData> LoadoutPresetResolver::Resolve(
    const LoadoutPresetData& data, PresetResolveContext& context
) const {
    ResolvedLoadoutPresetData resolved;

    for (std::size_t slotIndex = 0; slotIndex < data.weaponSlots.size(); ++slotIndex) {
        auto snapshot = WeaponPresetSerializer::ResolveLink(data.weaponSlots[slotIndex], appDataRoot_, context);
        if (!snapshot.success) {
            return PresetResolveFailure<ResolvedLoadoutPresetData>(
                "Weapon - " + std::string(LoadoutPresetData::K_WEAPON_SLOT_LABELS[slotIndex]), std::move(snapshot)
            );
        }
        resolved.weapons[slotIndex] = std::move(snapshot.value);
    }

    for (std::size_t slotIndex = 0; slotIndex < data.armorSlots.size(); ++slotIndex) {
        auto snapshot = ArmorPresetSerializer::ResolveLink(data.armorSlots[slotIndex], appDataRoot_, context);
        const std::string component = "Armor - " + std::string(LoadoutPresetData::K_ARMOR_SLOT_LABELS[slotIndex]);
        if (!snapshot.success) return PresetResolveFailure<ResolvedLoadoutPresetData>(component, std::move(snapshot));
        if (snapshot.value) {
            if (static_cast<std::size_t>(snapshot.value->passport.Slot_30_7561CB484566A4512003EA96ED44F88D) !=
                slotIndex) {
                snapshot.error = "A saved armor piece no longer matches its slot.";
                return PresetResolveFailure<ResolvedLoadoutPresetData>(component, std::move(snapshot));
            }
            resolved.armor[slotIndex] = std::move(snapshot.value);
        }
    }

    return ResolvedPreset(std::move(resolved));
}

PresetResolveResult<ResolvedLoadoutPresetData> LoadoutPresetResolver::Resolve(const LoadoutPresetData& data) const {
    PresetResolveContext context;
    return Resolve(data, context);
}

PresetResolveResult<ResolvedLoadoutPresetData> LoadoutPresetResolver::Resolve(
    const PresetLink<LoadoutPresetData>& link, PresetResolveContext& context
) const {
    return LoadoutPresetSerializer::ResolveLinkAs<ResolvedLoadoutPresetData>(
        link, appDataRoot_, context, [this](const LoadoutPresetData& data, PresetResolveContext& nestedContext) {
            return Resolve(data, nestedContext);
        }
    );
}

PresetResolveResult<ResolvedLoadoutPresetData> LoadoutPresetResolver::Resolve(
    const PresetLink<LoadoutPresetData>& link
) const {
    PresetResolveContext context;
    return Resolve(link, context);
}
