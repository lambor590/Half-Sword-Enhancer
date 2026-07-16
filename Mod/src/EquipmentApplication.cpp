#include "Utils/EquipmentApplication.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Hooks/GameHook.h"
#include "Logger.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/GameClass.h"
#include "Utils/PresetApplication.h"
#include "Utils/PresetUtils.h"
#include "Utils/Spawner.h"
#include "SDK/BP_Armor_Master_classes.hpp"
#include "SDK/BP_Armor_Modular_Core_Master_classes.hpp"
#include "SDK/Enum_MaterialLayer_structs.hpp"
#include "SDK/Enum_Weapon_Material_Type_structs.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

namespace EquipmentApplication {
    namespace {
        Logger g_logger{"EquipmentApplication"};

        struct ResolvedWeaponSlot {
            SDK::UStaticMesh* gripMesh = nullptr;
            std::optional<WeaponPresetData> preset;
        };

        struct ResolvedArmorSlot {
            SDK::FStr_Passport_Armor1 passport{};
            SDK::FLinearColor color3{0.5f, 0.5f, 0.5f, 1.0f};
            ArmorPresetData preset;
        };

        struct ResolvedLoadout {
            std::array<ResolvedWeaponSlot, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weapons{};
            std::vector<ResolvedArmorSlot> armor;
        };

        struct TrackedArmorRuntime {
            SDK::FStr_Passport_Armor1 passport{};
            ArmorRuntimeProps props{};
        };

        using TrackedArmorSet = std::array<std::optional<TrackedArmorRuntime>, LoadoutPresetData::K_ARMOR_SLOT_COUNT>;

        struct ApplyState;
        using ApplyFinish = std::function<void(bool success)>;

        std::unordered_map<SDK::AWillie_BP_C*, std::shared_ptr<ApplyState>>& ActiveApplications() {
            static std::unordered_map<SDK::AWillie_BP_C*, std::shared_ptr<ApplyState>> applications;
            return applications;
        }

        std::unordered_map<SDK::AWillie_BP_C*, TrackedArmorSet>& ArmorRuntimeState() {
            static std::unordered_map<SDK::AWillie_BP_C*, TrackedArmorSet> state;
            return state;
        }

        bool IsUsableWillie(SDK::AWillie_BP_C* willie) {
            return willie && SDK::UKismetSystemLibrary::IsValid(willie) && !willie->IsActorBeingDestroyed();
        }

        bool IsUsableActor(SDK::AActor* actor) {
            return actor && SDK::UKismetSystemLibrary::IsValid(actor) && !actor->IsActorBeingDestroyed();
        }

        bool IsIntrinsicArmorClass(const SDK::UClass* armorClass) {
            return armorClass && armorClass->GetName().find("BP_Armor_Legs_Panties") != std::string::npos;
        }

        bool IsRemovableArmor(const SDK::FStr_Passport_Armor1& passport) {
            auto* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
            return armorClass && !IsIntrinsicArmorClass(armorClass);
        }

        bool ResolveStaticMeshPath(const std::string& path, SDK::UStaticMesh*& result) {
            result = nullptr;
            if (path.empty()) return true;

            std::string normalized = path;
            if (normalized.front() != '/') normalized = "/Game/" + normalized;
            if (normalized.find('.') == std::string::npos) {
                const auto slash = normalized.rfind('/');
                if (slash != std::string::npos) normalized += "." + normalized.substr(slash + 1);
            }

            std::wstring widePath;
            if (!PresetUtils::TryUtf8ToWide(normalized, widePath)) return false;
            const auto softPath = SDK::UKismetSystemLibrary::MakeSoftObjectPath(SDK::FString(widePath.c_str()));
            const auto softReference = SDK::UKismetSystemLibrary::Conv_SoftObjPathToSoftObjRef(softPath);
            auto* loaded = SDK::UKismetSystemLibrary::LoadAsset_Blocking(softReference);
            if (!loaded || !loaded->IsA(SDK::UStaticMesh::StaticClass())) return false;
            result = static_cast<SDK::UStaticMesh*>(loaded);
            return true;
        }

        std::optional<WeaponPresetData> SnapshotWeaponActor(SDK::AActor* actor) {
            if (!IsUsableActor(actor) || !GameClass::IsModularWeapon(actor)) return std::nullopt;

            auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
            auto snapshot = PresetApplication::SnapshotWeaponPassport(weapon->Weapon_Passport);
            if (!snapshot) return std::nullopt;

            const auto objectPath = [](SDK::UObject* object) {
                return PresetUtils::ObjectToAbsolutePath(object);
            };
            SDK::UStaticMeshComponent* components[] = {weapon->Head, weapon->Guard, weapon->Grip, weapon->Pommel};
            for (std::size_t index = 0; index < std::size(components); ++index) {
                auto* component = components[index];
                if (!component) continue;

                auto& mesh = snapshot->meshPresets[index];
                SDK::USceneComponent* transformSource = component;
                SDK::UObject* asset = nullptr;
                if (!component->IsVisible()) {
                    for (auto* child : component->AttachChildren) {
                        if (!child || !child->IsVisible() || !child->IsA(SDK::USkeletalMeshComponent::StaticClass()))
                            continue;
                        auto* skeletal = static_cast<SDK::USkeletalMeshComponent*>(child);
                        asset = skeletal->GetSkeletalMeshAsset();
                        if (!asset) continue;
                        mesh.meshType = MeshType::Skeletal;
                        transformSource = skeletal;
                        break;
                    }
                }
                if (!asset) {
                    asset = component->StaticMesh;
                    mesh.meshType = MeshType::Static;
                    transformSource = component;
                }
                if (!asset) continue;

                mesh.meshPath = objectPath(asset);
                if (mesh.meshPath.empty()) continue;
                mesh.enabled = true;
                mesh.scale = transformSource->RelativeScale3D;
                mesh.rotation = transformSource->RelativeRotation;
                mesh.offset = transformSource->RelativeLocation;
            }

            auto& runtime = snapshot->runtimeProps;
            runtime.rigidity = {true, weapon->Rigidity};
            runtime.edgeSharpness = {true, weapon->Edge_Sharpness};
            runtime.rawDamage = {true, weapon->Raw_Damage};
            runtime.cuttingRate = {true, weapon->Cutting_Rate};
            runtime.stabRate = {true, weapon->Stab_Rate};
            runtime.defRating = {true, weapon->Def_Rating};
            runtime.gripRate = {true, weapon->Grip_Rate};
            runtime.drawCutRate = {true, weapon->Draw_Cut_Rate};
            runtime.tipSharpness = {true, weapon->Tip_Sharpness};
            runtime.kickPower = {true, weapon->Kick_Power};
            runtime.matDensity = {true, weapon->Mat_Density};
            runtime.dismemberSharp = {true, weapon->Dismemberment_Level_Sharp};
            runtime.dismemberBlunt = {true, weapon->Dismemberment_Level_Blunt};
            runtime.doubleEdged = {true, weapon->Double_Edged};
            runtime.piercing = {true, weapon->Piercing};
            runtime.noStab = {true, weapon->NoStab};
            runtime.staminaBurnR = {true, weapon->R_Hand_Stamina_Burn_Rate};
            runtime.staminaBurnL = {true, weapon->L_Hand_Stamina_Burn_Rate};
            runtime.staminaBurn2H = {true, weapon->TwoH_Default_Stamina_Burn_Rate};
            runtime.staminaBurn2HAlt = {true, weapon->TwoH_Alt_Stamina_Burn_Rate};
            return snapshot;
        }

        void OverlayWeaponAppearanceFromSlot(SDK::FStr_Passport_Weapon1& passport, const SDK::FStr_WeaponParts& slot) {
            for (auto it = begin(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81);
                 it != end(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81); ++it) {
                switch (it->Key()) {
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator0:
                        passport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator1:
                        passport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator2:
                        passport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = it->Value();
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator3:
                        passport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = it->Value();
                        break;
                    default: break;
                }
            }
            for (auto it = begin(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0);
                 it != end(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0); ++it) {
                if (it->Key() == SDK::Enum_Weapon_Material_Type::NewEnumerator2)
                    passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = it->Value();
                else if (it->Key() == SDK::Enum_Weapon_Material_Type::NewEnumerator3)
                    passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = it->Value();
            }
        }

        SDK::FStr_Passport_Weapon1 BuildWeaponPassportFromSlot(const SDK::FStr_WeaponParts& slot) {
            SDK::FStr_Passport_Weapon1 passport{};
            passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC =
                slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
            passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 =
                slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F;
            passport.GuardModule_13_6DD2B06245505E53B529D090333012F0 =
                slot.GuardModule_21_774015784EB0300D2671C894D57ED144;
            passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 =
                slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867;
            passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 =
                slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984;
            passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D =
                slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0;
            passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 =
                slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980;
            passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA;
            passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D;
            passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
            passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E =
                slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524;
            passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
            passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
            passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
            passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
            passport.MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36 = static_cast<SDK::Enum_MaterialLayer>(3);
            passport.MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2 = static_cast<SDK::Enum_MaterialLayer>(0);
            passport.MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A = static_cast<SDK::Enum_MaterialLayer>(14);
            passport.MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB = static_cast<SDK::Enum_MaterialLayer>(10);
            passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
            passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
            OverlayWeaponAppearanceFromSlot(passport, slot);
            passport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(4);
            passport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = 100.0;
            PresetApplication::NormalizeWeaponPassport(passport);
            return passport;
        }

        void OverlayConfiguredWeaponSlot(WeaponPresetData& snapshot, const SDK::FStr_WeaponParts& slot) {
            const auto oldPassport = snapshot.passport;
            const auto configured = BuildWeaponPassportFromSlot(slot);
            const bool sameGeometry = oldPassport.WeaponClass_54_B478ECF7499977809745A3973AD678EC ==
                                          configured.WeaponClass_54_B478ECF7499977809745A3973AD678EC &&
                                      oldPassport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 ==
                                          configured.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 &&
                                      oldPassport.GuardModule_13_6DD2B06245505E53B529D090333012F0 ==
                                          configured.GuardModule_13_6DD2B06245505E53B529D090333012F0 &&
                                      oldPassport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 ==
                                          configured.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 &&
                                      oldPassport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 ==
                                          configured.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6;
            if (!sameGeometry)
                for (auto& mesh : snapshot.meshPresets)
                    mesh = {};
            else {
                if (oldPassport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 !=
                    configured.HeadSize_21_2D425E61473B8F64FBAB51B223459D57)
                    snapshot.meshPresets[0] = {};
                if (oldPassport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 !=
                    configured.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704)
                    snapshot.meshPresets[1] = {};
                if (oldPassport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E !=
                    configured.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E)
                    snapshot.meshPresets[3] = {};
            }

            snapshot.passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC =
                configured.WeaponClass_54_B478ECF7499977809745A3973AD678EC;
            snapshot.passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 =
                configured.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139;
            snapshot.passport.GuardModule_13_6DD2B06245505E53B529D090333012F0 =
                configured.GuardModule_13_6DD2B06245505E53B529D090333012F0;
            snapshot.passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 =
                configured.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4;
            snapshot.passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 =
                configured.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6;
            snapshot.passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D =
                configured.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D;
            snapshot.passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 =
                configured.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9;
            snapshot.passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 =
                configured.HeadSize_21_2D425E61473B8F64FBAB51B223459D57;
            snapshot.passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 =
                configured.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704;
            snapshot.passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E =
                configured.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E;
            OverlayWeaponAppearanceFromSlot(snapshot.passport, slot);

            const auto objectPath = [](SDK::UObject* object) {
                return PresetUtils::ObjectToAbsolutePath(object);
            };
            snapshot.classPaths = {
                objectPath(slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066),
                objectPath(slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F),
                objectPath(slot.GuardModule_21_774015784EB0300D2671C894D57ED144),
                objectPath(slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867),
                objectPath(slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984),
                objectPath(slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0),
                objectPath(slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980),
            };
            snapshot.gripMeshPath = objectPath(slot.GripMesh_39_EDA3307B485303C5BF981B82D8462D0A);
            snapshot.coaInt = slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239;
            PresetApplication::NormalizeWeaponPassport(snapshot.passport);
        }

        bool ResolveWeaponPreset(const WeaponPresetData& source, ResolvedWeaponSlot& result, std::string& error) {
            auto preset = source;
            if (!PresetApplication::MaterializeWeaponPreset(preset, &error)) return false;
            if (!ResolveStaticMeshPath(preset.gripMeshPath, result.gripMesh)) {
                error = "The saved grip appearance is unavailable";
                return false;
            }

            result.preset = std::move(preset);
            return true;
        }

        bool ResolveLoadout(const ResolvedLoadoutPresetData& source, ResolvedLoadout& result, std::string& error) {
            for (std::size_t index = 0; index < source.weapons.size(); ++index) {
                if (!source.weapons[index]) continue;
                if (ResolveWeaponPreset(*source.weapons[index], result.weapons[index], error)) continue;
                error.insert(0, "A weapon in this loadout: ");
                return false;
            }

            const auto armorCount =
                std::ranges::count_if(source.armor, [](const auto& slot) { return slot.has_value(); });
            result.armor.reserve(static_cast<std::size_t>(armorCount));
            for (std::size_t index = 0; index < source.armor.size(); ++index) {
                if (!source.armor[index]) continue;
                auto preset = *source.armor[index];
                if (!PresetApplication::MaterializeArmorPreset(preset, &error)) {
                    error.insert(0, "An armor piece in this loadout: ");
                    return false;
                }
                const auto slot = preset.passport.Slot_30_7561CB484566A4512003EA96ED44F88D;
                auto* armorClass = preset.passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
                if (static_cast<std::size_t>(slot) != index || IsIntrinsicArmorClass(armorClass)) {
                    error = "A saved armor piece does not fit its loadout slot";
                    return false;
                }
                result.armor.push_back({
                    .passport = preset.passport,
                    .color3 = preset.passport.FabricColor3_89_167D399343950DE18CC2F9AC76D99042,
                    .preset = std::move(preset),
                });
            }
            return true;
        }

        void WriteWeaponAppearanceToSlot(SDK::FStr_WeaponParts& slot, const SDK::FStr_Passport_Weapon1* passport) {
            for (auto it = begin(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81);
                 it != end(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81); ++it) {
                switch (it->Key()) {
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator0:
                        it->Value() = passport ? passport->MaterialMetalSteel_37_AB7A28C94B176CF81A6C8BA34AC57C36
                                               : static_cast<SDK::Enum_MaterialLayer>(3);
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator1:
                        it->Value() = passport ? passport->MaterialMetalColored_39_DC2EAC244758A8D82855CC940784A1D2
                                               : static_cast<SDK::Enum_MaterialLayer>(0);
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator2:
                        it->Value() = passport ? passport->MaterialWeood_41_E0B3C8DB48943B878AEFA3AB01E7B99A
                                               : static_cast<SDK::Enum_MaterialLayer>(14);
                        break;
                    case SDK::Enum_Weapon_Material_Type::NewEnumerator3:
                        it->Value() = passport ? passport->MaterialLeather_43_41D1114148FDB4FE4DACC8A2F4CA9FEB
                                               : static_cast<SDK::Enum_MaterialLayer>(10);
                        break;
                    default: break;
                }
            }
            for (auto it = begin(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0);
                 it != end(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0); ++it) {
                if (it->Key() == SDK::Enum_Weapon_Material_Type::NewEnumerator2)
                    it->Value() = passport ? passport->ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743
                                           : SDK::FLinearColor{0.4f, 0.26f, 0.13f, 1.0f};
                else if (it->Key() == SDK::Enum_Weapon_Material_Type::NewEnumerator3)
                    it->Value() = passport ? passport->ColorLeather_48_DC45F07E4C0C3280278212A7158EE638
                                           : SDK::FLinearColor{0.3f, 0.18f, 0.08f, 1.0f};
            }
        }

        void WriteResolvedWeaponSlot(SDK::FStr_WeaponParts& slot, const ResolvedWeaponSlot& source) {
            const auto* passport = source.preset ? &source.preset->passport : nullptr;
            const SDK::FVector defaultSize{1.0, 1.0, 1.0};
            slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 =
                passport ? passport->WeaponClass_54_B478ECF7499977809745A3973AD678EC : nullptr;
            slot.GripMesh_39_EDA3307B485303C5BF981B82D8462D0A = source.gripMesh;
            slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 =
                passport ? passport->GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4 : nullptr;
            slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F =
                passport ? passport->HeadModule_11_62DF53134688807E1DA7F4A20E9F7139 : nullptr;
            slot.GuardModule_21_774015784EB0300D2671C894D57ED144 =
                passport ? passport->GuardModule_13_6DD2B06245505E53B529D090333012F0 : nullptr;
            slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 =
                passport ? passport->PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6 : nullptr;
            slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 =
                passport ? passport->HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D : nullptr;
            slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 =
                passport ? passport->HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9 : nullptr;
            slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA =
                passport ? passport->HeadSize_21_2D425E61473B8F64FBAB51B223459D57 : defaultSize;
            slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D =
                passport ? passport->GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 : defaultSize;
            slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 =
                passport ? passport->PommelSize_27_660CC00C49C26D503E16B2BC58CE115E : defaultSize;
            slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = source.preset ? source.preset->coaInt : 0;
            WriteWeaponAppearanceToSlot(slot, passport);
        }

        std::array<SDK::AActor*, LoadoutPresetData::K_WEAPON_SLOT_COUNT> GetWeaponActors(
            const SDK::AWillie_BP_C& willie
        ) {
            return {
                willie.Weapon_R,        willie.Weapon_L,        willie.Weapon_Slot_R_1,  willie.Weapon_Slot_R_2,
                willie.Weapon_Slot_L_1, willie.Weapon_Slot_L_2, willie.Weapon_Slot_Back,
            };
        }

        void ClearLoadEquipmentArmor(SDK::AWillie_BP_C& willie) {
            auto& map = willie.Load_Equipment.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                            .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
            for (auto it = begin(map); it != end(map); ++it) {
                auto& entry = it->Value();
                if (IsIntrinsicArmorClass(entry.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3)) continue;
                entry.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 = nullptr;
                entry.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F = {0.5f, 0.5f, 0.5f, 1.0f};
                entry.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B = {0.5f, 0.5f, 0.5f, 1.0f};
                entry.Color3_9_D8B5A08742A87F5492F8138A4F686141 = {0.5f, 0.5f, 0.5f, 1.0f};
            }
        }

        bool RemoveAllArmor(SDK::AWillie_BP_C& willie) {
            std::array<SDK::EArmorSlots_Enum, LoadoutPresetData::K_ARMOR_SLOT_COUNT> slots{};
            std::size_t slotCount = 0;
            for (auto it = begin(willie.Currently_Equipped_Armor); it != end(willie.Currently_Equipped_Armor); ++it) {
                if (IsRemovableArmor(it->Value()) && slotCount < slots.size()) slots[slotCount++] = it->Key();
            }
            for (std::size_t index = 0; index < slotCount; ++index) {
                SDK::FTransform transform{};
                transform.Scale3D = {1.0, 1.0, 1.0};
                SDK::ABP_Armor_Modular_Core_Master_C* dropped = nullptr;
                willie.Remove_Armor(transform, slots[index], nullptr, &dropped);
                if (IsUsableActor(dropped)) dropped->K2_DestroyActor();
            }
            for (auto it = begin(willie.Currently_Equipped_Armor); it != end(willie.Currently_Equipped_Armor); ++it)
                if (IsRemovableArmor(it->Value())) return false;
            return true;
        }

        void ResetLoadEquipmentArmor(SDK::AWillie_BP_C& willie, const ResolvedLoadout& loadout) {
            auto& map = willie.Load_Equipment.Armor_84_A1BA4DD44FD262BCA53B9DACF03CDF04
                            .ArmorinSlots_31_702A9C5C40C7F4335C6B4687EC09936A;
            ClearLoadEquipmentArmor(willie);
            for (const auto& source : loadout.armor) {
                for (auto it = begin(map); it != end(map); ++it) {
                    if (it->Key() != source.passport.Slot_30_7561CB484566A4512003EA96ED44F88D) continue;
                    auto& entry = it->Value();
                    entry.ArmorBPClass_2_0A22459840BF9E6989DFA4BA6CFED1D3 =
                        source.passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
                    entry.Color1_5_5527FC7C442DCF594A4DA5BA8D94351F =
                        source.passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393;
                    entry.Color2_7_1FF790D94C8CD95FF2D76183E7102E1B =
                        source.passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C;
                    entry.Color3_9_D8B5A08742A87F5492F8138A4F686141 = source.color3;
                    break;
                }
            }
        }

        void PublishArmorRuntimeState(SDK::AWillie_BP_C* willie, const ResolvedLoadout& loadout) {
            TrackedArmorSet tracked{};
            for (const auto& armor : loadout.armor) {
                const int slot = static_cast<int>(armor.passport.Slot_30_7561CB484566A4512003EA96ED44F88D);
                if (slot < 0 || slot >= static_cast<int>(tracked.size())) continue;
                auto identity = PresetApplication::SnapshotArmorPassport(armor.passport);
                if (!identity) continue;
                tracked[static_cast<std::size_t>(slot)] =
                    TrackedArmorRuntime{identity->passport, armor.preset.runtimeProps};
            }
            if (std::ranges::any_of(tracked, [](const auto& value) { return value.has_value(); }))
                ArmorRuntimeState()[willie] = tracked;
            else
                ArmorRuntimeState().erase(willie);
        }

        bool EquipResolvedWeaponSlot(SDK::AWillie_BP_C* willie, int slotIndex, const ResolvedWeaponSlot& resolved) {
            if (!willie || (slotIndex != 0 && slotIndex != 1)) return false;
            const auto* preset = resolved.preset ? &*resolved.preset : nullptr;
            auto* weaponClass = preset ? preset->passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC : nullptr;
            if (!weaponClass) {
                if (slotIndex == 0)
                    willie->Set_Up_Right_Hand_Weapon(nullptr, willie->Weapon_R, false, true, {});
                else
                    willie->Set_Up_Left_Hand_Weapon(nullptr, willie->Weapon_L, false, true, {});
                return true;
            }

            auto passport = preset->passport;
            if (slotIndex == 0)
                willie->Set_Up_Right_Hand_Weapon(weaponClass, willie->Weapon_R, false, true, passport);
            else
                willie->Set_Up_Left_Hand_Weapon(weaponClass, willie->Weapon_L, false, true, passport);

            auto* actor = slotIndex == 0 ? static_cast<SDK::AActor*>(willie->Weapon_R)
                                         : static_cast<SDK::AActor*>(willie->Weapon_L);
            if (!IsUsableActor(actor) || !GameClass::IsModularWeapon(actor)) return false;
            static_cast<SDK::AModularWeaponBP_C*>(actor)->Weapon_Passport = passport;
            if (PresetApplication::ApplyWeaponMeshOverrides(actor, preset->meshPresets) &&
                PresetApplication::ApplyWeaponRuntimeOverrides(actor, preset->runtimeProps))
                return true;

            if (slotIndex == 0)
                willie->Set_Up_Right_Hand_Weapon(nullptr, willie->Weapon_R, false, true, {});
            else
                willie->Set_Up_Left_Hand_Weapon(nullptr, willie->Weapon_L, false, true, {});
            return false;
        }

        void ClearSheathedWeaponActors(SDK::AWillie_BP_C& willie) {
            const auto actors = GetWeaponActors(willie);
            auto* rightHand = actors[0];
            auto* leftHand = actors[1];
            willie.Event_Clear_Sheathed_Weapon_Slots();
            for (std::size_t index = 2; index < actors.size(); ++index) {
                auto* actor = actors[index];
                if (actor == rightHand || actor == leftHand) continue;
                if (IsUsableActor(actor)) actor->K2_DestroyActor();
            }
        }

        void WriteWeaponConfiguration(SDK::AWillie_BP_C& willie, const ResolvedLoadout& loadout) {
            auto& weapons = willie.Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
            for (int index = 0; index < static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT); ++index)
                WriteResolvedWeaponSlot(LoadoutPresetData::GetWeaponSlot(weapons, index), loadout.weapons[index]);
        }

        bool RebuildWeaponActors(
            SDK::UWorld* world, SDK::AWillie_BP_C& willie, const ResolvedLoadout& loadout,
            std::optional<int> editedSlot = std::nullopt
        ) {
            if (!world || (editedSlot && (*editedSlot < 0 ||
                                          *editedSlot >= static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT))))
                return false;

            const bool rebuildHands = !editedSlot || *editedSlot < 2;
            const bool rebuildSheaths = !editedSlot || *editedSlot >= 2;
            bool success = true;
            if (rebuildHands) {
                if (editedSlot)
                    success = EquipResolvedWeaponSlot(&willie, *editedSlot, loadout.weapons[*editedSlot]);
                else
                    success = EquipResolvedWeaponSlot(&willie, 0, loadout.weapons[0]) &&
                              EquipResolvedWeaponSlot(&willie, 1, loadout.weapons[1]);
            }

            if (rebuildSheaths) {
                ClearSheathedWeaponActors(willie);
                constexpr std::array<SDK::ESheathSlots_Enum, 5> K_SHEATH_SLOTS = {
                    SDK::ESheathSlots_Enum::NewEnumerator0, SDK::ESheathSlots_Enum::NewEnumerator1,
                    SDK::ESheathSlots_Enum::NewEnumerator2, SDK::ESheathSlots_Enum::NewEnumerator3,
                    SDK::ESheathSlots_Enum::NewEnumerator4,
                };
                const auto transform = willie.GetTransform();
                for (std::size_t index = 2; index < loadout.weapons.size(); ++index) {
                    const auto& resolved = loadout.weapons[index];
                    if (!resolved.preset || !resolved.preset->passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC)
                        continue;

                    auto* actor =
                        Spawner::SpawnCustomizableFromPassport(world, resolved.preset->passport, transform, false);
                    if (!IsUsableActor(actor) || !GameClass::IsModularWeapon(actor)) {
                        if (IsUsableActor(actor)) actor->K2_DestroyActor();
                        success = false;
                        continue;
                    }
                    auto* weapon = static_cast<SDK::AModularWeaponBP_C*>(actor);
                    willie.Sheathe_on_Spawn(weapon, K_SHEATH_SLOTS[index - 2]);
                    weapon->Weapon_Passport = resolved.preset->passport;
                    if (!PresetApplication::ApplyWeaponMeshOverrides(actor, resolved.preset->meshPresets) ||
                        !PresetApplication::ApplyWeaponRuntimeOverrides(actor, resolved.preset->runtimeProps)) {
                        if (IsUsableActor(actor)) actor->K2_DestroyActor();
                        success = false;
                    }
                }
            }

            if (editedSlot) {
                auto& weapons = willie.Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                WriteResolvedWeaponSlot(
                    LoadoutPresetData::GetWeaponSlot(weapons, *editedSlot), loadout.weapons[*editedSlot]
                );
            } else {
                WriteWeaponConfiguration(willie, loadout);
            }
            if (!success && rebuildSheaths) ClearSheathedWeaponActors(willie);
            return success;
        }

        struct ApplyState {
            SDK::UWorld* world = nullptr;
            SDK::AWillie_BP_C* willie = nullptr;
            ResolvedLoadout target;
            std::size_t armorIndex = 0;
            bool replaceWeapons = true;
            bool stepQueued = false;
            bool finished = false;
            ApplyFinish finish;
        };

        bool IsCurrentApplication(const std::shared_ptr<ApplyState>& state) {
            const auto active = ActiveApplications().find(state->willie);
            return active != ActiveApplications().end() && active->second == state;
        }

        void FinishApplication(const std::shared_ptr<ApplyState>& state, bool success) {
            if (state->finished) return;
            state->finished = true;
            const auto active = ActiveApplications().find(state->willie);
            if (active != ActiveApplications().end() && active->second == state) ActiveApplications().erase(active);
            auto finish = std::move(state->finish);
            if (finish) finish(success);
        }

        void QueueApplyStep(const std::shared_ptr<ApplyState>& state);

        void QueueApplyStep(const std::shared_ptr<ApplyState>& state) {
            if (state->stepQueued || state->finished) return;
            state->stepQueued = true;
            const bool queued = GameHook::QueueAction([state](const RuntimeContextSnapshot& runtime) {
                state->stepQueued = false;
                if (!IsCurrentApplication(state) || runtime.world != state->world || !IsUsableWillie(state->willie)) {
                    FinishApplication(state, false);
                    return;
                }

                auto& equipment = state->target;
                if (state->armorIndex >= equipment.armor.size()) {
                    if (!equipment.armor.empty()) state->willie->Set_Up_Armor(true, false);
                    const bool success =
                        !state->replaceWeapons || RebuildWeaponActors(state->world, *state->willie, equipment);
                    if (success) PublishArmorRuntimeState(state->willie, equipment);
                    FinishApplication(state, success);
                    return;
                }

                auto& armor = equipment.armor[state->armorIndex];
                bool runtimeApplied = true;
                auto prePickupProps = armor.preset.runtimeProps;
                const bool deferPickupDisable = prePickupProps.pickUp.enabled && !prePickupProps.pickUp.value;
                if (deferPickupDisable) prePickupProps.pickUp.enabled = false;

                SDK::AActor* spawned = nullptr;
                const bool pickedUp = Spawner::SpawnAndEquipArmor(
                    runtime.world, state->willie, armor.passport,
                    [&runtimeApplied, &spawned, prePickupProps](SDK::AActor* actor) {
                        spawned = actor;
                        runtimeApplied = PresetApplication::ApplyArmorRuntimeOverrides(actor, prePickupProps);
                    }
                );
                if (pickedUp && runtimeApplied && deferPickupDisable && IsUsableActor(spawned))
                    runtimeApplied = PresetApplication::ApplyArmorRuntimeOverrides(spawned, armor.preset.runtimeProps);

                if (pickedUp && runtimeApplied) {
                    ++state->armorIndex;
                    QueueApplyStep(state);
                    return;
                }

                g_logger.Log(
                    "armor apply failed: slot=%d",
                    static_cast<int>(armor.passport.Slot_30_7561CB484566A4512003EA96ED44F88D)
                );
                FinishApplication(state, false);
            });
            if (!queued) {
                state->stepQueued = false;
                FinishApplication(state, false);
            }
        }

        bool BeginApplication(
            SDK::UWorld* world, SDK::AWillie_BP_C* willie, ResolvedLoadout target, bool replaceWeapons,
            ApplyFinish finish, std::string* error
        ) {
            if (!world || !IsUsableWillie(willie)) {
                if (error) *error = "Enter a map with an active character";
                return false;
            }
            if (ActiveApplications().contains(willie)) {
                if (error) *error = "Equipment is already being changed for this character";
                return false;
            }

            if (!RemoveAllArmor(*willie)) {
                if (error) *error = "The character's current armor could not be removed";
                return false;
            }
            ResetLoadEquipmentArmor(*willie, target);

            auto state = std::make_shared<ApplyState>();
            state->world = world;
            state->willie = willie;
            state->target = std::move(target);
            state->replaceWeapons = replaceWeapons;
            state->finish = std::move(finish);
            ActiveApplications()[willie] = state;
            QueueApplyStep(state);
            if (error) error->clear();
            return true;
        }

        struct PendingNPCInitialization {
            SDK::UWorld* world = nullptr;
            SDK::AWillie_BP_C* npc = nullptr;
            std::optional<ResolvedLoadoutPresetData> loadout;
            LoadoutApplyCallback onComplete;
            int ticksRemaining = 2;
        };

        void QueueNPCInitialization(const std::shared_ptr<PendingNPCInitialization>& state) {
            const bool queued = GameHook::QueueAction([state](const RuntimeContextSnapshot& runtime) {
                if (runtime.world != state->world || !IsUsableWillie(state->npc)) {
                    if (state->onComplete) state->onComplete(false);
                    return;
                }
                if (--state->ticksRemaining > 0) {
                    QueueNPCInitialization(state);
                    return;
                }
                if (!state->loadout) {
                    if (state->onComplete) state->onComplete(true);
                    return;
                }

                std::string error;
                auto completion = std::move(state->onComplete);
                if (!ApplyPlayerLoadout(state->world, state->npc, *state->loadout, &error, completion)) {
                    g_logger.Log("linked NPC loadout could not start: %s", error.c_str());
                    if (completion) completion(false);
                }
            });
            if (!queued) {
                auto completion = std::move(state->onComplete);
                if (completion) completion(false);
            }
        }
    }

    SDK::FStr_Passport_Weapon1 DefaultWeaponPassport() {
        SDK::FStr_Passport_Weapon1 passport{};
        passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57 = {1.0, 1.0, 1.0};
        passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704 = {1.0, 1.0, 1.0};
        passport.GripSize_25_AC1660814C4C25C521AAA8830FE8ECCF = {1.0, 1.0, 1.0};
        passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E = {1.0, 1.0, 1.0};
        passport.CustomMassScaleHead_30_B95872A242AD944E2CE4D493F718F9D7 = 1.0;
        passport.CustomMassScaleGuard_51_3A9024E74306B7BB5D186087011D1927 = 1.0;
        passport.CustomMassScaleGrip_32_0EAADEE0419C05C6DB38F0AE134A9B10 = 1.0;
        passport.CustomMassScalePommel_34_0AB28D814BDEF17D408D0DAA3A453173 = 1.0;
        passport.ColorWood_46_F3AE05AD4495EBCD1D354C8025D7C743 = {0.4f, 0.26f, 0.13f, 1.0f};
        passport.ColorLeather_48_DC45F07E4C0C3280278212A7158EE638 = {0.3f, 0.18f, 0.08f, 1.0f};
        passport.Tier_67_05026E6F43B7300AA8BACC9D9F9AB461 = static_cast<SDK::Enum_Ranks>(4);
        passport.Price_60_83FE5A624EA188485BBE4E9C8606AEE5 = 100.0;
        PresetApplication::NormalizeWeaponPassport(passport);
        return passport;
    }

    void WriteWeaponPassportToSlot(const SDK::FStr_Passport_Weapon1& passport, SDK::FStr_WeaponParts& slot) {
        slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 =
            passport.WeaponClass_54_B478ECF7499977809745A3973AD678EC;
        slot.GripMesh_39_EDA3307B485303C5BF981B82D8462D0A = nullptr;
        slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = passport.HeadModule_11_62DF53134688807E1DA7F4A20E9F7139;
        slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = passport.GuardModule_13_6DD2B06245505E53B529D090333012F0;
        slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = passport.GripModule_18_F4DF51EB4E742195B8C6BAB17E4C5DB4;
        slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 =
            passport.PommelModule_15_561B01324BFCD4360DAE9A95299BB9D6;
        slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 =
            passport.HeadSubModule1_7_ABBFD017411F42A4950B1C9F2360A30D;
        slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 =
            passport.HeadSubModule2_9_90AAA8304C7794E1BF814C9354A1A7E9;
        slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = passport.HeadSize_21_2D425E61473B8F64FBAB51B223459D57;
        slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = passport.GuardSize_23_5A1AA0E04708E86FEFF61E974DDA8704;
        slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 =
            passport.PommelSize_27_660CC00C49C26D503E16B2BC58CE115E;
        WriteWeaponAppearanceToSlot(slot, &passport);
    }

    bool WriteWeaponPresetToSlot(WeaponPresetData& preset, SDK::FStr_WeaponParts& slot, std::string* error) {
        ResolvedWeaponSlot resolved;
        std::string resolveError;
        if (!ResolveWeaponPreset(preset, resolved, resolveError) || !resolved.preset) {
            if (error) *error = std::move(resolveError);
            return false;
        }
        preset = *resolved.preset;
        WriteResolvedWeaponSlot(slot, resolved);
        if (error) error->clear();
        return true;
    }

    void ClearWeaponSlot(SDK::FStr_WeaponParts& slot) {
        WriteResolvedWeaponSlot(slot, {});
    }

    bool EquipWeaponSlot(SDK::AWillie_BP_C* willie, int slotIndex, const SDK::FStr_WeaponParts& slot) {
        if (!willie || (slotIndex != 0 && slotIndex != 1)) return false;
        auto* weaponClass = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066;
        auto passport = BuildWeaponPassportFromSlot(slot);
        if (slotIndex == 0)
            willie->Set_Up_Right_Hand_Weapon(weaponClass, willie->Weapon_R, false, true, passport);
        else
            willie->Set_Up_Left_Hand_Weapon(weaponClass, willie->Weapon_L, false, true, passport);
        return weaponClass == nullptr || IsUsableActor(
                                             slotIndex == 0 ? static_cast<SDK::AActor*>(willie->Weapon_R)
                                                            : static_cast<SDK::AActor*>(willie->Weapon_L)
                                         );
    }

    bool CaptureEquippedWeaponPreset(SDK::AWillie_BP_C* willie, int handIndex, WeaponPresetData& result) {
        if (!willie || (handIndex != 0 && handIndex != 1)) return false;
        auto* actor =
            handIndex == 0 ? static_cast<SDK::AActor*>(willie->Weapon_R) : static_cast<SDK::AActor*>(willie->Weapon_L);
        auto snapshot = SnapshotWeaponActor(actor);
        if (!snapshot) return false;
        const auto& weapons = willie->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        OverlayConfiguredWeaponSlot(*snapshot, LoadoutPresetData::GetWeaponSlot(weapons, handIndex));
        result = std::move(*snapshot);
        return true;
    }

    bool CaptureConfiguredWeaponPreset(SDK::AWillie_BP_C* willie, int slotIndex, WeaponPresetData& result) {
        if (!willie || slotIndex < 0 || slotIndex >= static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT))
            return false;
        const auto& weapons = willie->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        const auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
        const auto actors = GetWeaponActors(*willie);
        if (auto snapshot = SnapshotWeaponActor(actors[static_cast<std::size_t>(slotIndex)])) {
            if (slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066) OverlayConfiguredWeaponSlot(*snapshot, slot);
            result = std::move(*snapshot);
            return true;
        }
        if (!slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066) return false;

        result = {};
        result.name = "Current weapon";
        result.id = "runtime-weapon-snapshot";
        result.passport = BuildWeaponPassportFromSlot(slot);
        OverlayConfiguredWeaponSlot(result, slot);
        return !result.classPaths.weaponClass.empty();
    }

    bool CaptureEquippedArmorPreset(SDK::AWillie_BP_C* willie, SDK::EArmorSlots_Enum slot, ArmorPresetData& result) {
        if (!willie) return false;
        for (auto it = begin(willie->Currently_Equipped_Armor); it != end(willie->Currently_Equipped_Armor); ++it) {
            if (it->Key() != slot || !IsRemovableArmor(it->Value())) continue;
            auto snapshot = PresetApplication::SnapshotArmorPassport(it->Value());
            if (!snapshot) return false;

            const int slotIndex = static_cast<int>(slot);
            const auto owner = ArmorRuntimeState().find(willie);
            if (slotIndex >= 0 && slotIndex < static_cast<int>(LoadoutPresetData::K_ARMOR_SLOT_COUNT) &&
                owner != ArmorRuntimeState().end()) {
                const auto& tracked = owner->second[static_cast<std::size_t>(slotIndex)];
                if (tracked && PresetApplication::ArmorPassportsEqual(tracked->passport, snapshot->passport))
                    snapshot->runtimeProps = tracked->props;
            }
            result = std::move(*snapshot);
            return true;
        }
        return false;
    }

    bool SynchronizeConfiguredWeaponActors(
        SDK::UWorld* world, SDK::AWillie_BP_C* willie, int overrideSlot, const WeaponPresetData* overridePreset,
        std::string* error
    ) {
        if (!world || !IsUsableWillie(willie) || overrideSlot < 0 ||
            overrideSlot >= static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT)) {
            if (error) *error = "The selected character or weapon slot is no longer available";
            return false;
        }

        ResolvedLoadout target;
        std::string resolveError;
        if (overridePreset) {
            if (!ResolveWeaponPreset(
                    *overridePreset, target.weapons[static_cast<std::size_t>(overrideSlot)], resolveError
                )) {
                if (error) *error = std::move(resolveError);
                return false;
            }
        } else {
            const auto& weapons = willie->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
            const auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, overrideSlot);
            if (!slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066) {
                target.weapons[static_cast<std::size_t>(overrideSlot)] = {};
            } else {
                WeaponPresetData configured;
                if (!CaptureConfiguredWeaponPreset(willie, overrideSlot, configured) ||
                    !ResolveWeaponPreset(
                        configured, target.weapons[static_cast<std::size_t>(overrideSlot)], resolveError
                    )) {
                    if (error) *error = resolveError.empty() ? "The equipped weapon could not be read" : resolveError;
                    return false;
                }
            }
        }

        if (RebuildWeaponActors(world, *willie, target, overrideSlot)) {
            if (error) error->clear();
            return true;
        }
        if (error) *error = "The equipped weapon could not be updated";
        return false;
    }

    void ClearWeaponActors(SDK::AWillie_BP_C* willie) {
        if (!IsUsableWillie(willie)) return;
        willie->Set_Up_Right_Hand_Weapon(nullptr, willie->Weapon_R, false, true, {});
        willie->Set_Up_Left_Hand_Weapon(nullptr, willie->Weapon_L, false, true, {});
        ClearSheathedWeaponActors(*willie);
    }

    bool ApplyPlayerLoadout(
        SDK::UWorld* world, SDK::AWillie_BP_C* player, const ResolvedLoadoutPresetData& loadout, std::string* error,
        LoadoutApplyCallback onComplete
    ) {
        ResolvedLoadout target;
        std::string resolveError;
        if (!ResolveLoadout(loadout, target, resolveError)) {
            if (error) *error = std::move(resolveError);
            return false;
        }
        return BeginApplication(
            world, player, std::move(target), true,
            [onComplete = std::move(onComplete)](bool success) {
                if (onComplete) onComplete(success);
            },
            error
        );
    }

    bool ApplyPlayerArmorSet(
        SDK::UWorld* world, SDK::AWillie_BP_C* player, const std::vector<ArmorPresetData>& armor, std::string* error,
        LoadoutApplyCallback onComplete
    ) {
        ResolvedLoadout target;
        std::array<bool, LoadoutPresetData::K_ARMOR_SLOT_COUNT> seen{};
        target.armor.reserve(armor.size());
        for (const auto& source : armor) {
            auto preset = source;
            std::string materializeError;
            if (!PresetApplication::MaterializeArmorPreset(preset, &materializeError)) {
                if (error) *error = "The selected armor is unavailable: " + materializeError;
                return false;
            }
            const int slot = static_cast<int>(preset.passport.Slot_30_7561CB484566A4512003EA96ED44F88D);
            auto* armorClass = preset.passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
            if (slot < 0 || slot >= static_cast<int>(seen.size()) || seen[static_cast<std::size_t>(slot)] ||
                IsIntrinsicArmorClass(armorClass)) {
                if (error) *error = "Two armor pieces use the same slot or cannot be equipped";
                return false;
            }
            seen[static_cast<std::size_t>(slot)] = true;
            target.armor.push_back({
                .passport = preset.passport,
                .color3 = preset.passport.FabricColor3_89_167D399343950DE18CC2F9AC76D99042,
                .preset = std::move(preset),
            });
        }

        return BeginApplication(world, player, std::move(target), false, std::move(onComplete), error);
    }

    bool WaitForNPCInitialization(
        SDK::UWorld* world, SDK::AWillie_BP_C* npc, LoadoutApplyCallback onComplete, std::string* error
    ) {
        if (!world || !IsUsableWillie(npc)) {
            if (error) *error = "The NPC is no longer available";
            return false;
        }
        auto state = std::make_shared<PendingNPCInitialization>();
        state->world = world;
        state->npc = npc;
        state->onComplete = std::move(onComplete);
        QueueNPCInitialization(state);
        if (error) error->clear();
        return true;
    }

    bool ApplyNPCLoadout(
        SDK::UWorld* world, SDK::AWillie_BP_C* npc, ResolvedLoadoutPresetData loadout, std::string* error,
        LoadoutApplyCallback onComplete
    ) {
        if (!world || !IsUsableWillie(npc)) {
            if (error) *error = "The NPC is no longer available";
            return false;
        }
        auto state = std::make_shared<PendingNPCInitialization>();
        state->world = world;
        state->npc = npc;
        state->loadout = std::move(loadout);
        state->onComplete = std::move(onComplete);
        QueueNPCInitialization(state);
        if (error) error->clear();
        return true;
    }

    void AbortRuntimeTransactionsForShutdown() noexcept {
        try {
            std::vector<std::shared_ptr<ApplyState>> active;
            active.reserve(ActiveApplications().size());
            for (const auto& [willie, state] : ActiveApplications()) {
                (void)willie;
                if (state) active.push_back(state);
            }
            ActiveApplications().clear();
            for (const auto& state : active)
                FinishApplication(state, false);
        } catch (...) {
            g_logger.Log("Exception while aborting equipment operations");
        }
    }

    void OnRuntimeShutdown() noexcept {
        AbortRuntimeTransactionsForShutdown();
        try {
            ArmorRuntimeState().clear();
        } catch (...) {
            g_logger.Log("Exception while clearing armor runtime snapshots");
        }
    }
}
