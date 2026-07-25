#pragma once

#include <cstdint>
#include <vector>

#include "Menu/Section.h"
#include "Menu/Keybind.h"
#include "SDK/Engine_structs.hpp"

namespace SDK {
    class AWeapon_Feet_C;
    class UPrimitiveComponent;
}

class PlayerAbilitiesSection : public Section {
public:
    static constexpr SectionDefinition SECTION{
        MenuTab::Player, "Abilities", "Customize how your character moves, fights, and recovers."
    };

    struct Config {
        int infiniteStaminaKey = 0x49; // I
        int enemyInfiniteStaminaKey = -1;
        int infiniteConsciousnessKey = -1;
        int enemyInfiniteConsciousnessKey = -1;
        int getUpKey = -1;
        int possessWillieKey = -1;
        int invulnerabilityKey = -1;
        int noPainKey = -1;
        int noKickCooldownKey = -1;
        int kickMultiplierKey = -1;
        int knockbackMultiplierKey = -1;
        int enemyNoPainKey = -1;
        int ragdollKey = -1;
        int enemyRagdollKey = -1;
        int jumpKey = 0x4A;        // J
        int playerSpeedKey = 0x50; // P
        int playerStrengthKey = -1;
        int bodyTonusKey = -1;
        int dashKey = -1;
        int biteAttackKey = -1;
        int enemyBiteKey = -1;
        int enemyBiteAllKey = -1;
        int enemyDrunkKey = -1;
        int boneControlKey = -1;
        int enemyBoneControlKey = -1;
        int enemyBreakBonesKey = -1;
        int consciousnessMultiplierKey = -1;
        int enemyConsciousnessMultiplierKey = -1;

        float jumpForce = 5000.0f;
        float playerRunMultiplier = 1.0f;
        float playerWalkMultiplier = 1.0f;
        float playerStrengthMultiplier = 1.0f;
        float playerGrabForceMultiplier = 1.0f;
        float playerHandsRigidityMultiplier = 1.0f;
        float bodyTonusAllBodyMultiplier = 1.0f;
        float dashForce = 7000.0f;
        float biteRange = 300.0f;
        float biteAllRange = 500.0f;
        float enemyDrunkLevel = 1.0f;
        float kickPowerMultiplier = 1.0f;
        float knockbackMultiplier = 1.0f;
        float consciousnessMultiplier = 1.0f;
        float enemyConsciousnessMultiplier = 1.0f;
        float boneBreakStrengthMultiplier = 1.0f;
        float enemyBoneBreakStrengthMultiplier = 1.0f;
        float boneMassMultiplier = 1.0f;
        float enemyBoneMassMultiplier = 1.0f;

        bool bodyTonusNoBodyWeakening = false;
        bool kickMultiplierAffectsEnemies = false;
        bool knockbackAffectsEnemies = false;
        bool blockBoneDislocation = true;
        bool blockEnemyBoneDislocation = true;
    };

private:
    Config cfg;
    KeybindList keybinds;
    struct KickWindow {
        SDK::AWeapon_Feet_C* foot = nullptr;
        SDK::UPrimitiveComponent* pendingImpulseComponent = nullptr;
        SDK::FVector pendingImpulse{};
        SDK::FVector pendingImpulseLocation{};
        SDK::FName pendingImpulseBone{};
        std::uint8_t pendingImpulseStep = 0;
        bool left = false;
        bool impulseSpent = false;
    };
    std::vector<KickWindow> kickWindows;

    void InitKeybinds();
    template <bool playerScope> void AddBoneControl();

    KickWindow* FindKickWindow(SDK::AWeapon_Feet_C* foot) noexcept;
    static void ClearPendingKickImpulse(KickWindow& window) noexcept;
    static void ApplyPendingKickImpulse(KickWindow& window);
    void OpenKickWindow(bool leftKick, GameHook::ProcessEventContext& context);
    void CloseKickWindow(bool leftKick, GameHook::ProcessEventContext& context);
    void HandleKickHit(GameHook::ProcessEventContext& context);
    void HandlePunchHit(GameHook::ProcessEventContext& context);
    static void TogglePossession(const RuntimeContextSnapshot& runtime);

public:
    explicit PlayerAbilitiesSection(ModContext& ctx);
    void Render() override;
    KeybindList* GetSearchKeybinds() noexcept override { return &keybinds; }
};
