#pragma once

#include <optional>
#include <span>
#include <string>

#include "Utils/ArmorPresetSerializer.h"
#include "Utils/PlayerEditorOverrides.h"
#include "Utils/WeaponPresetSerializer.h"

namespace SDK {
    class AActor;
    class AWillie_BP_C;
    struct FVector;
}

namespace PresetApplication {
    // Removes indeterminate SDK padding before a weapon passport is compared or serialized.
    void NormalizeWeaponPassport(SDK::FStr_Passport_Weapon1& passport) noexcept;
    [[nodiscard]] std::optional<WeaponPresetData> SnapshotWeaponPassport(const SDK::FStr_Passport_Weapon1& passport);
    // Removes the SDK blocked-slot map from the returned transport-safe passport.
    [[nodiscard]] std::optional<ArmorPresetData> SnapshotArmorPassport(const SDK::FStr_Passport_Armor1& passport);
    [[nodiscard]] bool ArmorPassportsEqual(
        const SDK::FStr_Passport_Armor1& left, const SDK::FStr_Passport_Armor1& right
    ) noexcept;
    // Game-thread only: resolves and type-checks every weapon Blueprint class before
    // publishing the passport to gameplay code.
    [[nodiscard]] bool MaterializeWeaponPreset(WeaponPresetData& preset, std::string* error = nullptr);
    // Game-thread only: resolves the armor core and restores its default TMap-backed blocked-slot data.
    [[nodiscard]] bool MaterializeArmorPreset(ArmorPresetData& preset, std::string* error = nullptr);
    [[nodiscard]] float PlayerScaleFromHeight(double heightRate) noexcept;
    [[nodiscard]] bool ApplyPlayerHeight(SDK::AWillie_BP_C* player, double heightRate);
    [[nodiscard]] bool ApplyPlayerWeight(SDK::AWillie_BP_C* player, double muscleRate);
    // Game-thread only: reapplies the Willie's spawn-time body setup, then normalizes
    // attached meshes as soon as that latent setup has updated the body scale.
    [[nodiscard]] bool ApplyLivePlayerHeight(SDK::AWillie_BP_C* player, double heightRate);
    [[nodiscard]] bool ApplyLivePlayerHeight(
        SDK::AWillie_BP_C* player, double heightRate, const SDK::FVector& finalActorScale
    );
    [[nodiscard]] bool ShouldApplyLivePlayerBodyOverrides(
        bool enforceOverrides, const PlayerEditorOverrides& current, const PlayerEditorOverrides& published
    ) noexcept;
    [[nodiscard]] bool ApplyPlayerOverrides(SDK::AWillie_BP_C* player, const PlayerEditorOverrides& overrides);
    [[nodiscard]] bool ApplyLivePlayerOverrides(
        SDK::AWillie_BP_C* player, const PlayerEditorOverrides& overrides
    );
    [[nodiscard]] bool ApplyWeaponRuntimeOverrides(
        SDK::AActor* actor, const WeaponPresetData::WeaponRuntimeProps& overrides
    );
    [[nodiscard]] bool ApplyWeaponMeshOverrides(
        SDK::AActor* actor, std::span<const MeshOverridePreset> overrides, bool enableSkeletalCollision = true
    );
    [[nodiscard]] bool ApplyArmorRuntimeOverrides(SDK::AActor* actor, const ArmorRuntimeProps& overrides);
}
