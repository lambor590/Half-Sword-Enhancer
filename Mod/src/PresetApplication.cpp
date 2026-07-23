#include "Utils/PresetApplication.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "SDK/Engine_classes.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/Modular_Weapon_Part_Master_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/GameClass.h"
#include "Utils/PresetUtils.h"
#include "Utils/Spawner.h"

namespace PresetApplication {
    namespace {
        struct WeaponClassSpec {
            const std::string WeaponClassPaths::* path;
            std::string_view baseName;
            const char* label;
            bool required;
            SDK::UClass* SDK::FStr_Passport_Weapon1::* destination;
        };

        constexpr std::array<WeaponClassSpec, 7> K_WEAPON_CLASS_SPECS{{
            {
                &WeaponClassPaths::weaponClass,
                "ModularWeaponBP_C",
                "weapon body",
                true,
                &SDK::FStr_Passport_Weapon1::WeaponClass_54_B478ECF7499977809745A3973AD678EC,
            },
            {
                &WeaponClassPaths::headModule,
                "Modular_Weapon_Part_Master_C",
                "head",
                true,
                &SDK::FStr_Passport_Weapon1::HeadModule_11_62DF53134688807E1DA7F4A20E9F7139,
            },
            {
                &WeaponClassPaths::guardModule,
                "Modular_Weapon_Part_Master_C",
                "guard",
                false,
                &SDK::FStr_Passport_Weapon1::GuardModule_13_6DD2B06245505E53B529D090333012F0,
            },
            {
                &WeaponClassPaths::gripModule,
                "Modular_Weapon_Part_Master_C",
                "grip",
                true,
                &SDK::FStr_Passport_Weapon1::GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4,
            },
            {
                &WeaponClassPaths::pommelModule,
                "Modular_Weapon_Part_Master_C",
                "pommel",
                false,
                &SDK::FStr_Passport_Weapon1::PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6,
            },
            {
                &WeaponClassPaths::subModule1,
                "Modular_Weapon_Part_Master_C",
                "extra part 1",
                false,
                &SDK::FStr_Passport_Weapon1::HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D,
            },
            {
                &WeaponClassPaths::subModule2,
                "Modular_Weapon_Part_Master_C",
                "extra part 2",
                false,
                &SDK::FStr_Passport_Weapon1::HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9,
            },
        }};

        SDK::UObject* LoadMeshAsset(const MeshOverridePreset& preset) {
            if (!preset.enabled || preset.meshPath.empty()) return nullptr;

            std::string path = preset.meshPath;
            if (path[0] != '/') path = "/Game/" + path;
            if (path.find('.') == std::string::npos) {
                const auto slash = path.rfind('/');
                if (slash != std::string::npos) path += "." + path.substr(slash + 1);
            }

            std::wstring widePath;
            if (!PresetUtils::TryUtf8ToWide(path, widePath)) return nullptr;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftObjectPath(SDK::FString(widePath.c_str()));
            const auto softReference = SDK::UKismetSystemLibrary::Conv_SoftObjPathToSoftObjRef(softPath);
            auto* loaded = SDK::UKismetSystemLibrary::LoadAsset_Blocking(softReference);
            if (!loaded) return nullptr;

            auto* expectedClass = preset.meshType == MeshType::Skeletal ? SDK::USkeletalMesh::StaticClass()
                                                                        : SDK::UStaticMesh::StaticClass();
            return loaded->IsA(expectedClass) ? loaded : nullptr;
        }
    }

    void NormalizeWeaponPassport(SDK::FStr_Passport_Weapon1& passport) noexcept {
        std::memset(passport.Pad_14, 0, sizeof(passport.Pad_14));
        std::memset(passport.Pad_EC, 0, sizeof(passport.Pad_EC));
        std::memset(reinterpret_cast<std::uint8_t*>(&passport) + 0xF9, 0, 7);
    }

    std::optional<WeaponPresetData> SnapshotWeaponPassport(const SDK::FStr_Passport_Weapon1& passport) {
        if (!passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC) return std::nullopt;

        WeaponPresetData snapshot;
        snapshot.name = "Current weapon";
        snapshot.id = "runtime-weapon-snapshot";
        snapshot.passport = passport;
        NormalizeWeaponPassport(snapshot.passport);
        snapshot.deferredWeaponName = snapshot.passport.Name_57_3729B51148E846FE8DD336B9419BCEE1.GetRawString();
        snapshot.classPaths = {
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC),
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139),
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.GuardModule_13_6DD2B06245505E53B529D090333012F0),
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4),
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6),
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D),
            PresetUtils::ObjectToAbsolutePath(snapshot.passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9),
        };

        return snapshot;
    }

    std::optional<ArmorPresetData> SnapshotArmorPassport(const SDK::FStr_Passport_Armor1& passport) {
        auto* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        if (!armorClass) return std::nullopt;

        ArmorPresetData snapshot;
        snapshot.name = "Current armor";
        snapshot.id = "runtime-armor-snapshot";
        snapshot.armorCorePath = PresetUtils::ObjectToAbsolutePath(armorClass);
        snapshot.passport = passport;
        snapshot.passport.SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51 = {};

        return snapshot;
    }

    bool ArmorPassportsEqual(const SDK::FStr_Passport_Armor1& left, const SDK::FStr_Passport_Armor1& right) noexcept {
        static constexpr std::size_t BEFORE_BLOCKED_SLOTS =
            offsetof(SDK::FStr_Passport_Armor1, SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51);
        static constexpr std::size_t AFTER_BLOCKED_SLOTS =
            offsetof(SDK::FStr_Passport_Armor1, RequiresModuleHirarchy_47_9ED58E2C48514BE5153606977BE68B6A);
        static constexpr std::size_t TAIL_SIZE = sizeof(SDK::FStr_Passport_Armor1) - AFTER_BLOCKED_SLOTS;

        return std::memcmp(&left, &right, BEFORE_BLOCKED_SLOTS) == 0 &&
               std::memcmp(
                   reinterpret_cast<const std::byte*>(&left) + AFTER_BLOCKED_SLOTS,
                   reinterpret_cast<const std::byte*>(&right) + AFTER_BLOCKED_SLOTS, TAIL_SIZE
               ) == 0;
    }

    bool MaterializeWeaponPreset(WeaponPresetData& preset, std::string* error) {
        std::array<SDK::UClass*, K_WEAPON_CLASS_SPECS.size()> resolvedClasses{};
        for (std::size_t index = 0; index < K_WEAPON_CLASS_SPECS.size(); ++index) {
            const auto& spec = K_WEAPON_CLASS_SPECS[index];
            const auto& path = preset.classPaths.*spec.path;
            if (path.empty()) {
                if (!spec.required) continue;
                if (error) *error = std::string("The preset is missing its ") + spec.label;
                return false;
            }
            auto* loadedClass = Spawner::LoadClass(path);
            if (!GameClass::IsSubclassOf(loadedClass, spec.baseName)) {
                if (error) *error = std::string("The preset's ") + spec.label + " is unavailable";
                return false;
            }
            resolvedClasses[index] = loadedClass;
        }

        auto& passport = preset.passport;
        NormalizeWeaponPassport(passport);
        for (std::size_t index = 0; index < K_WEAPON_CLASS_SPECS.size(); ++index)
            passport.*K_WEAPON_CLASS_SPECS[index].destination = resolvedClasses[index];
        if (!preset.deferredWeaponName.empty()) {
            std::wstring wideName;
            if (!PresetUtils::TryUtf8ToWide(preset.deferredWeaponName, wideName)) {
                if (error) *error = "The weapon name contains text that cannot be used";
                return false;
            }
            passport.Name_57_3729B51148E846FE8DD336B9419BCEE1 =
                SDK::BasicFilesImplUtils::StringToName(wideName.c_str());
        }
        return true;
    }

    bool MaterializeArmorPreset(ArmorPresetData& preset, std::string* error) {
        auto* armorClass = preset.armorCorePath.empty() ? nullptr : Spawner::LoadClass(preset.armorCorePath);
        if (!armorClass || !armorClass->ClassDefaultObject || !GameClass::IsArmor(armorClass->ClassDefaultObject)) {
            if (error) *error = "The selected armor is unavailable";
            return false;
        }

        auto* defaults = static_cast<SDK::ABP_Armor_Master_C*>(armorClass->ClassDefaultObject);
        const auto& passport = preset.passport;
        const auto validateModuleIndex = [error](int index, int available, std::string_view field) {
            if (index >= 0 && index <= available) return true;
            if (error) *error = "The selected " + std::string(field) + " is unavailable for this armor";
            return false;
        };
        if (GameClass::IsModularArmor(defaults)) {
            const auto* modular = static_cast<SDK::ABP_Armor_Modular_Core_Master_C*>(defaults);
            if (!validateModuleIndex(
                    passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4, modular->Available_Modules_1.Num(), "Module 1"
                ) ||
                !validateModuleIndex(
                    passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0, modular->Available_Modules_2.Num(), "Module 2"
                ) ||
                !validateModuleIndex(
                    passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2, modular->Available_Modules_3.Num(), "Module 3"
                ))
                return false;
        } else if (
            passport.Module1_5_46B7198E4341C93CBF6AE989EF9898E4 != 0 ||
            passport.Module2_7_5B7940B84CFD673B25103D96E0AFEEB0 != 0 ||
            passport.Module3_9_E282C465414F6D4EF2A8039FBA847AD2 != 0
        ) {
            if (error) *error = "This armor does not support selectable parts";
            return false;
        }
        const auto& defaultBlocked = defaults->Armor_Passport.SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51;
        preset.passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 = armorClass;
        preset.passport.SlotsBlocked_45_0807340240E57ACE5A59D39F5E998F51 = defaultBlocked;
        return true;
    }

    bool ApplyPlayerOverrides(SDK::AWillie_BP_C* player, const PlayerEditorOverrides& o) {
        if (!player) return false;

        if (o.heightRate.enabled) {
            player->Height_Rate = o.heightRate.value;
            player->Character_Passport.Height_21_0EB204DF4978B92AD0ED188FD32EEC7B = o.heightRate.value;
        }
        if (o.muscleRate.enabled) {
            player->Muscle_Rate = o.muscleRate.value;
            player->Character_Passport.Weight_23_65E4C6534D14653F96EB739F159E58CD = o.muscleRate.value;
        }
        if (o.scaleMutationInhibitor.enabled) player->Scale_Mutation_Inhibitor = o.scaleMutationInhibitor.value;

        if (o.health.enabled) player->Health = o.health.value;
        if (o.headHealth.enabled) player->Head_Health = o.headHealth.value;
        if (o.neckHealth.enabled) player->Neck_Health = o.neckHealth.value;
        if (o.armRHealth.enabled) player->Arm_R_Health = o.armRHealth.value;
        if (o.armLHealth.enabled) player->Arm_L_Health = o.armLHealth.value;
        if (o.bodyUpperHealth.enabled) player->Body_Upper_Health = o.bodyUpperHealth.value;
        if (o.bodyLowerHealth.enabled) player->Body_Lower_Health = o.bodyLowerHealth.value;
        if (o.legRHealth.enabled) player->Leg_R_Health = o.legRHealth.value;
        if (o.legLHealth.enabled) player->Leg_L_Health = o.legLHealth.value;
        if (o.backHealth.enabled) player->Back_Health = o.backHealth.value;
        if (o.consciousness.enabled) player->Consciousness = o.consciousness.value;
        if (o.regenRate.enabled) player->Regen_Rate = o.regenRate.value;

        if (o.allBodyTonus.enabled) player->All_Body_Tonus = o.allBodyTonus.value;
        if (o.headTonus.enabled) player->Head_Tonus = o.headTonus.value;
        if (o.armRTonus.enabled) player->Arm_R_Tonus = o.armRTonus.value;
        if (o.armLTonus.enabled) player->Arm_L_Tonus = o.armLTonus.value;
        if (o.legRTonus.enabled) player->Leg_R_Tonus = o.legRTonus.value;
        if (o.legLTonus.enabled) player->Leg_L_Tonus = o.legLTonus.value;
        if (o.musclePower.enabled) player->Muscle_Power = o.musclePower.value;
        if (o.orientationStrength.enabled) player->Orientation_Strength = o.orientationStrength.value;
        if (o.angularStrength.enabled) player->Angular_Strength = o.angularStrength.value;
        if (o.hitRigidity.enabled) player->Hit_Rigidity = o.hitRigidity.value;

        if (o.runningSpeedRate.enabled) player->Running_Speed_Rate = o.runningSpeedRate.value;
        if (o.walkSpeedRateRun.enabled) player->Walk_Speed_Rate_Run = static_cast<float>(o.walkSpeedRateRun.value);
        if (o.jumpRate.enabled) player->Jump_Rate = o.jumpRate.value;
        if (o.dodgeRate.enabled) player->Dodge_Rate = o.dodgeRate.value;
        if (o.crawlRate.enabled) player->Crawl_Rate = o.crawlRate.value;
        if (o.getUpRate.enabled) player->Get_Up_Rate = o.getUpRate.value;
        if (o.fallenRate.enabled) player->Fallen_Rate = o.fallenRate.value;

        if (o.damageRate.enabled) player->Damage_Rate__Additional_ = o.damageRate.value;
        if (o.limbDamageRate.enabled) player->Limb_Damage_Rate__Additional_ = o.limbDamageRate.value;
        if (o.dismemberThreshold.enabled) player->Health_Threshold_For_Dismemberment = o.dismemberThreshold.value;
        if (o.stamina.enabled) player->Stamina = o.stamina.value;
        if (o.staminaBurnSwingR.enabled) player->Stamina_Burn_Swing_R = o.staminaBurnSwingR.value;
        if (o.staminaBurnSwingL.enabled) player->Stamina_Burn_Swing_L = o.staminaBurnSwingL.value;
        if (o.staminaBurnDodge.enabled) player->Stamina_Burn_Dodge = o.staminaBurnDodge.value;
        if (o.grabForceR.enabled) player->R_Grab_Force_Limit = o.grabForceR.value;
        if (o.grabForceL.enabled) player->L_Grab_Force_Limit = o.grabForceL.value;
        if (o.handsRigidity.enabled) player->Hands_Rigidity__Gauntlets_ = o.handsRigidity.value;
        if (o.bodySkill.enabled) player->Body_Skill__Temp_ = o.bodySkill.value;
        if (o.weaponSkill.enabled) player->Weapon_Skill__Temp_ = o.weaponSkill.value;

        if (o.skillThrust.enabled) player->Skill_Unlock_Weapon_Thrust = o.skillThrust.value;
        if (o.skillParry.enabled) player->Skill_Unlock_Weapon_Parry = o.skillParry.value;
        if (o.skillAltGrip.enabled) player->Skill_Unlock_Weapon_Alt_Grip = o.skillAltGrip.value;
        if (o.skillAltStance.enabled) player->Skill_Unlock_Weapon_Alt_Stance = o.skillAltStance.value;
        if (o.skillRotate.enabled) player->Skill_Unlock_Weapon_Rotate = o.skillRotate.value;
        if (o.skillCrouch.enabled) player->Skill_Unlock_Body_Crouch = o.skillCrouch.value;
        if (o.skillDodge.enabled) player->Skill_Unlock_Body_Dodge = o.skillDodge.value;
        if (o.skillKick.enabled) player->Skill_Unlock_Body_Kick = o.skillKick.value;
        if (o.skillSlomo.enabled) player->Skill_Unlock_Body_Slomo = o.skillSlomo.value;

        if (o.exhaustion.enabled) player->Exhaustion = o.exhaustion.value;
        if (o.drunk.enabled) player->Drunk = o.drunk.value;
        if (o.fear.enabled) player->Fear = o.fear.value;
        if (o.invulnerable.enabled) {
            player->Invulnerable = o.invulnerable.value;
            player->BitPad_5C_0 = o.invulnerable.value;
        }
        return true;
    }

    bool ApplyWeaponRuntimeOverrides(SDK::AActor* actor, const WeaponPresetData::WeaponRuntimeProps& o) {
        if (!GameClass::IsModularWeapon(actor)) return false;
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);

        if (o.rigidity.enabled) weapon->Rigidity = o.rigidity.value;
        if (o.edgeSharpness.enabled) weapon->Edge_Sharpness = o.edgeSharpness.value;
        if (o.rawDamage.enabled) weapon->Raw_Damage = o.rawDamage.value;
        if (o.cuttingRate.enabled) weapon->Cutting_Rate = o.cuttingRate.value;
        if (o.stabRate.enabled) weapon->Stab_Rate = o.stabRate.value;
        if (o.defRating.enabled) weapon->Def_Rating = o.defRating.value;
        if (o.gripRate.enabled) weapon->Grip_Rate = o.gripRate.value;
        if (o.drawCutRate.enabled) weapon->Draw_Cut_Rate = o.drawCutRate.value;
        if (o.tipSharpness.enabled) weapon->Tip_Sharpness = o.tipSharpness.value;
        if (o.kickPower.enabled) weapon->Kick_Power = o.kickPower.value;
        if (o.matDensity.enabled) weapon->Mat_Density = o.matDensity.value;
        if (o.dismemberSharp.enabled) weapon->Dismemberment_Level_Sharp = o.dismemberSharp.value;
        if (o.dismemberBlunt.enabled) weapon->Dismemberment_Level_Blunt = o.dismemberBlunt.value;
        if (o.doubleEdged.enabled) weapon->Double_Edged = o.doubleEdged.value;
        if (o.piercing.enabled) weapon->Piercing = o.piercing.value;
        if (o.noStab.enabled) weapon->NoStab = o.noStab.value;
        if (o.staminaBurnR.enabled) weapon->R_Hand_Stamina_Burn_Rate = o.staminaBurnR.value;
        if (o.staminaBurnL.enabled) weapon->L_Hand_Stamina_Burn_Rate = o.staminaBurnL.value;
        if (o.staminaBurn2H.enabled) weapon->TwoH_Default_Stamina_Burn_Rate = o.staminaBurn2H.value;
        if (o.staminaBurn2HAlt.enabled) weapon->TwoH_Alt_Stamina_Burn_Rate = o.staminaBurn2HAlt.value;
        return true;
    }

    bool ApplyWeaponMeshOverrides(
        SDK::AActor* actor, std::span<const MeshOverridePreset> overrides, bool enableSkeletalCollision
    ) {
        if (!GameClass::IsModularWeapon(actor) || overrides.size() != WeaponPresetData::MODULE_SLOT_COUNT) return false;
        auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);

        std::array<SDK::UObject*, WeaponPresetData::MODULE_SLOT_COUNT> meshes{};
        for (std::size_t i = 0; i < overrides.size(); ++i) {
            if (!overrides[i].enabled) continue;
            meshes[i] = LoadMeshAsset(overrides[i]);
            if (!meshes[i]) return false;
        }

        SDK::UStaticMeshComponent* components[] = {weapon->Head, weapon->Guard, weapon->Grip, weapon->Pommel};
        for (std::size_t i = 0; i < overrides.size(); ++i) {
            auto* component = components[i];
            if (!component) {
                if (overrides[i].enabled) return false;
                continue;
            }

            const auto& preset = overrides[i];
            if (!preset.enabled) {
                component->SetVisibility(true, true);
                continue;
            }

            if (preset.meshType == MeshType::Static) {
                component->SetVisibility(true, true);
                component->SetStaticMesh(static_cast<SDK::UStaticMesh*>(meshes[i]));
                component->SetRelativeScale3D(preset.scale);
                component->K2_SetRelativeRotation(preset.rotation, false, nullptr, true);
                component->K2_SetRelativeLocation(preset.offset, false, nullptr, true);
                continue;
            }

            component->SetVisibility(false, true);
            auto* added = weapon->AddComponentByClass(
                SDK::USkeletalMeshComponent::StaticClass(), false, SDK::FTransform{}, false
            );
            if (!added) return false;

            auto* skeletal = static_cast<SDK::USkeletalMeshComponent*>(added);
            skeletal->SetSkeletalMeshAsset(static_cast<SDK::USkeletalMesh*>(meshes[i]));
            skeletal->SetAnimationMode(SDK::EAnimationMode::AnimationCustomMode, false);
            skeletal->SetRenderStatic(true);
            skeletal->SetSimulatePhysics(false);
            skeletal->SetEnableGravity(false);
            skeletal->SetComponentTickEnabled(false);
            skeletal->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);

            if (enableSkeletalCollision) {
                component->SetCollisionEnabled(SDK::ECollisionEnabled::NoCollision);
                skeletal->bAlwaysCreatePhysicsState = true;
                skeletal->bEnablePerPolyCollision = true;
                skeletal->SetCollisionProfileName(component->GetCollisionProfileName(), true);
                skeletal->SetCollisionEnabled(SDK::ECollisionEnabled::QueryAndPhysics);
                skeletal->SetSimulatePhysics(false);
                skeletal->SetEnableGravity(false);
            }

            skeletal->K2_AttachToComponent(
                component, SDK::FName(), SDK::EAttachmentRule::SnapToTarget, SDK::EAttachmentRule::SnapToTarget,
                SDK::EAttachmentRule::SnapToTarget, !enableSkeletalCollision
            );
            skeletal->SetRelativeScale3D(preset.scale);
            skeletal->K2_SetRelativeRotation(preset.rotation, false, nullptr, true);
            skeletal->K2_SetRelativeLocation(preset.offset, false, nullptr, true);
        }
        return true;
    }

    bool ApplyArmorRuntimeOverrides(SDK::AActor* actor, const ArmorRuntimeProps& o) {
        if (!GameClass::IsArmor(actor)) return false;
        auto* armor = static_cast<SDK::ABP_Armor_Master_C*>(actor);

        if (o.protectionBlunt.enabled) armor->Protection_Blunt = o.protectionBlunt.value;
        if (o.protectionCut.enabled) armor->Protection_Cut = o.protectionCut.value;
        if (o.protectionStab.enabled) armor->Protection_Stab = o.protectionStab.value;
        if (o.materialDensity.enabled) armor->Material_Density = o.materialDensity.value;
        if (o.massScale.enabled) armor->Mass_Scale = o.massScale.value;
        if (o.handsRigidity.enabled) armor->Hands_Rigidity__Gauntlets_ = o.handsRigidity.value;
        if (o.strapPower.enabled) armor->Strap_Power__Helmet_ = o.strapPower.value;
        if (o.aiInvincibilityRate.enabled) armor->AI_Invinvcibility_Rate = o.aiInvincibilityRate.value;
        if (o.price.enabled) armor->Price = o.price.value;
        if (o.pickUp.enabled) armor->Pick_Up = o.pickUp.value;
        return true;
    }
}
