#include "Menu/Sections/Equipment/LoadoutManagerSection.h"
#include "SDK/Willie_BP_classes.hpp"

#include "ConfigManager.h"
#include "Hooks/GameHook.h"
#include "Utils/ArmorGenerationUi.h"
#include "Utils/BlueprintRegistry.h"
#include "Utils/EquipmentApplication.h"
#include "Utils/EquipmentGenerator.h"
#include "Utils/GuiUtils.h"
#include "Utils/LoadoutPresetResolver.h"
#include "Utils/PresetApplication.h"
#include "Utils/WeaponGenerationUi.h"
#include "SDK/Enum_Weapon_Material_Type_structs.hpp"
#include "SDK/ModularWeaponBP_classes.hpp"

namespace {
    constexpr const char* LOADOUT_CONFIG_SECTION = "LoadoutManager";

    void RenderPresetComponentBadge(bool copied, bool broken, const std::string& diagnostic) {
        const ImVec4 color = broken ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                                    : (copied ? ImVec4(0.45f, 0.8f, 1.0f, 1.0f) : ImVec4(0.45f, 1.0f, 0.6f, 1.0f));
        const char* badge = broken ? "[Unavailable]" : (copied ? "[Copy]" : "[Reference]");
        ImGui::TextColored(color, "%s", badge);
        if (broken && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            ImGui::SetItemTooltip("%s", diagnostic.c_str());
    }

    void RenderPresetComponentDetail(bool broken, const std::string& diagnostic) {
        if (!broken || diagnostic.empty()) return;
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", diagnostic.c_str());
        ImGui::PopStyleColor();
    }

    bool IsArmorRemovable(const SDK::FStr_Passport_Armor1& passport) {
        auto* armorClass = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43;
        return armorClass && armorClass->GetName().find("BP_Armor_Legs_Panties") == std::string::npos;
    }

    struct WeaponSlotSnapshot {
        SDK::UClass* weaponClass = nullptr;
        SDK::UStaticMesh* gripMesh = nullptr;
        SDK::UClass* gripModule = nullptr;
        SDK::UClass* headModule = nullptr;
        SDK::UClass* guardModule = nullptr;
        SDK::UClass* pommelModule = nullptr;
        SDK::UClass* subModule1 = nullptr;
        SDK::UClass* subModule2 = nullptr;
        SDK::FVector headSize{};
        SDK::FVector guardSize{};
        SDK::FVector pommelSize{};
        int coa = 0;
        std::vector<std::pair<SDK::Enum_Weapon_Material_Type, SDK::Enum_MaterialLayer>> materials;
        std::vector<std::pair<SDK::Enum_Weapon_Material_Type, SDK::FLinearColor>> colors;

        explicit WeaponSlotSnapshot(const SDK::FStr_WeaponParts& slot)
            : weaponClass(slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066),
              gripMesh(slot.GripMesh_39_EDA3307B485303C5BF981B82D8462D0A),
              gripModule(slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867),
              headModule(slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F),
              guardModule(slot.GuardModule_21_774015784EB0300D2671C894D57ED144),
              pommelModule(slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984),
              subModule1(slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0),
              subModule2(slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980),
              headSize(slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA),
              guardSize(slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D),
              pommelSize(slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524),
              coa(slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239) {
            for (auto it = begin(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81);
                 it != end(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81); ++it)
                materials.emplace_back(it->Key(), it->Value());
            for (auto it = begin(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0);
                 it != end(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0); ++it)
                colors.emplace_back(it->Key(), it->Value());
        }

        void Restore(SDK::FStr_WeaponParts& slot) const {
            slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 = weaponClass;
            slot.GripMesh_39_EDA3307B485303C5BF981B82D8462D0A = gripMesh;
            slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = gripModule;
            slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = headModule;
            slot.GuardModule_21_774015784EB0300D2671C894D57ED144 = guardModule;
            slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = pommelModule;
            slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = subModule1;
            slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = subModule2;
            slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = headSize;
            slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = guardSize;
            slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = pommelSize;
            slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239 = coa;
            for (auto it = begin(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81);
                 it != end(slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81); ++it) {
                for (const auto& [key, value] : materials) {
                    if (key != it->Key()) continue;
                    it->Value() = value;
                    break;
                }
            }
            for (auto it = begin(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0);
                 it != end(slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0); ++it) {
                for (const auto& [key, value] : colors) {
                    if (key != it->Key()) continue;
                    it->Value() = value;
                    break;
                }
            }
        }
    };

} // namespace

const char* LoadoutManagerSection::ClassNameCache::Get(SDK::UClass* cls) {
    if (cls != ptr) {
        ptr = cls;
        name = cls ? BlueprintRegistry::CleanDisplayName(cls->GetName()) : "(empty)";
    }
    return name.c_str();
}

const char* LoadoutManagerSection::GetArmorSlotDisplayName(SDK::EArmorSlots_Enum slot) noexcept {
    const auto index = static_cast<std::size_t>(slot);
    return index < LoadoutPresetData::K_ARMOR_SLOT_LABELS.size()
               ? LoadoutPresetData::K_ARMOR_SLOT_LABELS[index].data()
               : "Other";
}

const char* LoadoutManagerSection::GetWeaponSlotDisplayName(int slot) noexcept {
    const auto index = static_cast<std::size_t>(slot);
    return index < LoadoutPresetData::K_WEAPON_SLOT_LABELS.size()
               ? LoadoutPresetData::K_WEAPON_SLOT_LABELS[index].data()
               : "Other";
}

void LoadoutManagerSection::EnsureModulePool() {
    if (modulePool.populated.load(std::memory_order_acquire)) return;
    bool expected = false;
    if (!modulePoolQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    const bool queued = GameHook::QueueAction([this](const RuntimeContextSnapshot&) {
        modulePool.Populate();
        modulePoolQueued.store(false, std::memory_order_release);
    });
    if (!queued) modulePoolQueued.store(false, std::memory_order_release);
}

bool LoadoutManagerSection::RenderVectorDrag(const char* label, SDK::FVector& vec) {
    float v[3] = {static_cast<float>(vec.X), static_cast<float>(vec.Y), static_cast<float>(vec.Z)};
    GuiUtils::DebouncedDragFloat3(label, v, 0.01f, 0.0f, 0.0f, "%.3f");
    const bool edited = ImGui::IsItemEdited();
    GuiUtils::StoreEdited(vec, v);
    return edited;
}

void LoadoutManagerSection::QueueArmorTransaction(
    std::string label, ArmorTransactionBuilder buildTarget, ArmorTransactionSuccess onSuccess,
    const RuntimeContextSnapshot* immediateRuntime
) {
    {
        std::lock_guard operationLock(equipmentOperationMutex);
        if (presetSaveState->inProgress.load(std::memory_order_acquire)) {
            QueueDraftResult("Wait for the loadout to finish saving");
            return;
        }
        if (loadoutApplyInProgress.load(std::memory_order_acquire)) {
            QueueDraftResult("Wait for the current loadout change to finish");
            return;
        }
        bool expected = false;
        if (!armorOperationInProgress.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            QueueDraftResult("Wait for the current armor change to finish");
            return;
        }
    }

    auto start = [this, label = std::move(label), buildTarget = std::move(buildTarget),
                  onSuccess = std::move(onSuccess)](const RuntimeContextSnapshot& runtime) mutable {
        if (!runtime.world || !runtime.player) {
            QueueDraftResult(label + " failed: no player is active");
            armorOperationInProgress.store(false, std::memory_order_release);
            return;
        }

        std::vector<ArmorPresetData> armor;
        armor.reserve(runtime.player->Currently_Equipped_Armor.Num());
        for (auto it = begin(runtime.player->Currently_Equipped_Armor);
             it != end(runtime.player->Currently_Equipped_Armor); ++it) {
            if (!IsArmorRemovable(it->Value())) continue;
            ArmorPresetData snapshot;
            if (!EquipmentApplication::CaptureEquippedArmorPreset(runtime.player, it->Key(), snapshot)) {
                QueueDraftResult(label + " failed: equipped armor is unavailable");
                armorOperationInProgress.store(false, std::memory_order_release);
                return;
            }
            armor.push_back(std::move(snapshot));
        }

        std::string error;
        if (!buildTarget(runtime, armor, error)) {
            QueueDraftResult(label + " failed: " + error);
            armorOperationInProgress.store(false, std::memory_order_release);
            return;
        }

        auto* player = runtime.player;
        auto completionLabel = label;
        const bool started = EquipmentApplication::ApplyPlayerArmorSet(
            runtime.world, player, armor, &error,
            [this, player, label = std::move(completionLabel), onSuccess = std::move(onSuccess)](bool success) mutable {
                if (success && onSuccess) onSuccess(player);
                QueueDraftResult(success ? std::string{} : label + " failed");
                armorOperationInProgress.store(false, std::memory_order_release);
            }
        );
        if (!started) {
            QueueDraftResult(label + " failed: " + error);
            armorOperationInProgress.store(false, std::memory_order_release);
        }
    };

    if (immediateRuntime)
        start(*immediateRuntime);
    else if (!GameHook::QueueAction(std::move(start))) {
        QueueDraftResult("Could not change armor");
        armorOperationInProgress.store(false, std::memory_order_release);
    }
}

void LoadoutManagerSection::ReapplyArmorSlot(SDK::EArmorSlots_Enum slot) {
    QueueArmorTransaction(
        "Update armor slot", [slot](const RuntimeContextSnapshot&, std::vector<ArmorPresetData>& armor,
                                    std::string& error) {
            const bool found = std::any_of(armor.begin(), armor.end(), [slot](const auto& preset) {
                return preset.passport.Slot_30_7561CB484566A4512003EA96ED44F88D == slot;
            });
            if (!found) {
                error = "that armor is no longer equipped";
                return false;
            }
            return true;
        }
    );
}

void LoadoutManagerSection::RemoveArmorForSlot(SDK::EArmorSlots_Enum slot) {
    const int slotIndex = static_cast<int>(slot);
    QueueArmorTransaction(
        "Remove armor from " + std::string(GetArmorSlotDisplayName(slot)),
        [slot](const RuntimeContextSnapshot&, std::vector<ArmorPresetData>& armor, std::string& error) {
            const auto oldSize = armor.size();
            std::erase_if(armor, [slot](const auto& preset) {
                return preset.passport.Slot_30_7561CB484566A4512003EA96ED44F88D == slot;
            });
            if (armor.size() == oldSize) {
                error = "that armor is no longer equipped";
                return false;
            }
            return true;
        },
        [this, slotIndex](SDK::AWillie_BP_C* player) { QueueArmorDraftUpdate(player, slotIndex, {}); }
    );
}

void LoadoutManagerSection::ApplyWeaponToPlayer(int slotIndex) {
    if (IsEquipmentBusy()) return;
    GameHook::QueueAction([this, slotIndex](const RuntimeContextSnapshot& runtime) {
        auto* player = runtime.player;
        if (!player || !runtime.world) return;
        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        const auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
        WeaponPresetData configuredPreset;
        const WeaponPresetData* overridePreset = nullptr;
        if (slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066) {
            if (!EquipmentApplication::CaptureConfiguredWeaponPreset(player, slotIndex, configuredPreset)) {
                QueueDraftResult(
                    "Could not update " + std::string(GetWeaponSlotDisplayName(slotIndex)) +
                    ": equipped weapon is unavailable"
                );
                return;
            }
            overridePreset = &configuredPreset;
        }
        std::string error;
        if (!EquipmentApplication::SynchronizeConfiguredWeaponActors(
                runtime.world, player, slotIndex, overridePreset, &error
            )) {
            QueueDraftResult("Could not update " + std::string(GetWeaponSlotDisplayName(slotIndex)) + ": " + error);
            return;
        }
        QueueDraftResult();
    });
}

void LoadoutManagerSection::StripAllArmor() {
    QueueArmorTransaction(
        "Remove removable armor", [](const RuntimeContextSnapshot&, std::vector<ArmorPresetData>& armor, std::string&) {
            armor.clear();
            return true;
        },
        [this](SDK::AWillie_BP_C* player) { QueueClearArmorDraftLinks(player); }
    );
}

void LoadoutManagerSection::ClearAllWeapons() {
    if (IsEquipmentBusy()) return;
    for (auto& state : weaponSlotLinkStates)
        state.Clear();
    GameHook::QueueAction([](const RuntimeContextSnapshot& runtime) {
        auto* player = runtime.player;
        if (!player) return;
        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        for (int i = 0; i < static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT); ++i) {
            auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
            EquipmentApplication::ClearWeaponSlot(slot);
        }
        EquipmentApplication::ClearWeaponActors(player);
    });
}

void LoadoutManagerSection::GenerateArmorForSlot(SDK::EArmorSlots_Enum slotEnum) {
    const int slotIndex = static_cast<int>(slotEnum);
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);
    auto options = cfg.armorOptions;

    QueueArmorTransaction(
        "Create armor for " + std::string(GetArmorSlotDisplayName(slotEnum)),
        [slotEnum, tier, options](const RuntimeContextSnapshot& runtime, std::vector<ArmorPresetData>& armor,
                                 std::string& error) {
            const auto passport = EquipmentGenerator::GenerateArmor(runtime.world, tier, slotEnum, options);
            auto generated = PresetApplication::SnapshotArmorPassport(passport);
            if (!generated) {
                error = "could not create valid armor";
                return false;
            }
            const auto existing = std::find_if(armor.begin(), armor.end(), [slotEnum](const auto& preset) {
                return preset.passport.Slot_30_7561CB484566A4512003EA96ED44F88D == slotEnum;
            });
            if (existing != armor.end())
                *existing = std::move(*generated);
            else
                armor.push_back(std::move(*generated));
            return true;
        },
        [this, slotIndex](SDK::AWillie_BP_C* player) { QueueArmorDraftUpdate(player, slotIndex, {}); }
    );
}

void LoadoutManagerSection::RandomizeAllArmor(const RuntimeContextSnapshot* immediateRuntime) {
    KeybindArmorConfig snapshot;
    if (immediateRuntime) {
        std::lock_guard lock(keybindConfigMutex);
        snapshot = keybindConfigSnapshot;
    } else {
        snapshot = {.tier = cfg.generateTier, .options = cfg.armorOptions};
    }
    const auto tier = static_cast<SDK::Enum_Ranks>(snapshot.tier);
    const auto options = snapshot.options;

    QueueArmorTransaction(
        "Randomize all armor",
        [tier, options](const RuntimeContextSnapshot& runtime, std::vector<ArmorPresetData>& armor,
                        std::string& error) {
            for (auto& current : armor) {
                const auto slot = current.passport.Slot_30_7561CB484566A4512003EA96ED44F88D;
                const auto passport = EquipmentGenerator::GenerateArmor(runtime.world, tier, slot, options);
                auto generated = PresetApplication::SnapshotArmorPassport(passport);
                if (!generated) {
                    error = "could not create armor for " +
                            std::string(LoadoutManagerSection::GetArmorSlotDisplayName(slot));
                    return false;
                }
                current = std::move(*generated);
            }
            return true;
        },
        [this](SDK::AWillie_BP_C* player) { QueueClearArmorDraftLinks(player); },
        immediateRuntime
    );
}

void LoadoutManagerSection::GenerateWeaponForSlot(int slotIndex) {
    if (IsEquipmentBusy()) return;
    auto tier = static_cast<SDK::Enum_Ranks>(cfg.generateTier);
    auto type = static_cast<SDK::Enum_WeaponType>(0);
    const bool generateGreatsword = WeaponGenerationUi::IsGreatswordIndex(cfg.weaponSpecificType);
    auto specificType = generateGreatsword ? WeaponGenerationUi::TWO_HANDED_SWORDS
                                           : WeaponGenerationUi::SpecificTypeFromIndex(cfg.weaponSpecificType);

    GameHook::QueueAction([this, slotIndex, tier, type, specificType,
                           generateGreatsword](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player || !runtime.world) return;
        auto passport = EquipmentGenerator::GenerateWeapon(runtime.world, type, tier, specificType, generateGreatsword);
        if (!EquipmentGenerator::IsPassportValid(passport)) {
            QueueDraftResult("Could not create a valid weapon");
            return;
        }
        auto generatedPreset = PresetApplication::SnapshotWeaponPassport(passport);
        if (!generatedPreset) {
            QueueDraftResult("Could not use the new weapon");
            return;
        }

        std::string applyError;
        if (!EquipmentApplication::SynchronizeConfiguredWeaponActors(
                runtime.world, runtime.player, slotIndex, &*generatedPreset, &applyError
            )) {
            QueueDraftResult(
                "Could not create a weapon for " + std::string(GetWeaponSlotDisplayName(slotIndex)) + ": " + applyError
            );
            return;
        }
        QueueWeaponDraftUpdate(runtime.player, slotIndex, {});
        QueueDraftResult();
    });
}

void LoadoutManagerSection::ImportWeaponPreset(int slotIndex) {
    if (IsEquipmentBusy()) return;
    if (!weaponPresetComposer.HasLink()) return;
    auto resolved = weaponPresetComposer.Resolve();
    if (!resolved.success || !resolved.value) {
        SetDraftError("Weapon preset: " + resolved.error);
        return;
    }
    auto data = std::move(*resolved.value);
    auto presetLink = weaponPresetComposer.GetLink();

    const bool queued = GameHook::QueueAction([this, slotIndex, data = std::move(data),
                                               presetLink = std::move(presetLink)](const RuntimeContextSnapshot& runtime) mutable {
        const char* slotName = GetWeaponSlotDisplayName(slotIndex);
        if (!runtime.player) {
            QueueDraftResult("Could not equip weapon preset in " + std::string(slotName) + ": no player is active");
            return;
        }
        std::string applyError;
        if (!EquipmentApplication::SynchronizeConfiguredWeaponActors(
                runtime.world, runtime.player, slotIndex, &data, &applyError
            )) {
            QueueDraftResult("Could not equip weapon preset in " + std::string(slotName) + ": " + applyError);
            return;
        }

        QueueWeaponDraftUpdate(runtime.player, slotIndex, std::move(presetLink));
        QueueDraftResult();
    });
    if (!queued) {
        SetDraftError("Could not start the weapon preset change");
    }
}

void LoadoutManagerSection::ImportArmorPreset(std::optional<SDK::EArmorSlots_Enum> expectedSlot) {
    if (!armorPresetComposer.HasLink()) return;
    auto resolved = armorPresetComposer.Resolve();
    if (!resolved.success || !resolved.value) {
        SetDraftError("Armor preset: " + resolved.error);
        return;
    }
    const auto slotEnum = resolved.value->passport.Slot_30_7561CB484566A4512003EA96ED44F88D;
    const int slotIndex = static_cast<int>(slotEnum);
    if (slotIndex < 0 || slotIndex >= static_cast<int>(armorSlotLinkStates.size())) {
        SetDraftError("Armor preset uses an unsupported slot");
        return;
    }
    if (expectedSlot && slotEnum != *expectedSlot) {
        SetDraftError(
            "Armor preset belongs to " + std::string(GetArmorSlotDisplayName(slotEnum)) +
            "; choose a preset for " + GetArmorSlotDisplayName(*expectedSlot)
        );
        return;
    }
    auto data = std::move(*resolved.value);
    auto presetLink = armorPresetComposer.GetLink();
    QueueArmorTransaction(
        "Equip armor preset in " + std::string(GetArmorSlotDisplayName(slotEnum)),
        [slotEnum, data = std::move(data)](const RuntimeContextSnapshot& runtime, std::vector<ArmorPresetData>& armor,
                                          std::string& error) mutable {
            for (auto current = begin(runtime.player->Currently_Equipped_Armor);
                 current != end(runtime.player->Currently_Equipped_Armor); ++current) {
                if (current->Key() == slotEnum && !IsArmorRemovable(current->Value())) {
                    error = "this armor is part of the character and cannot be replaced";
                    return false;
                }
            }
            const auto existing = std::find_if(armor.begin(), armor.end(), [slotEnum](const auto& preset) {
                return preset.passport.Slot_30_7561CB484566A4512003EA96ED44F88D == slotEnum;
            });
            if (existing != armor.end())
                *existing = std::move(data);
            else
                armor.push_back(std::move(data));
            return true;
        },
        [this, slotIndex, presetLink = std::move(presetLink)](SDK::AWillie_BP_C* player) mutable {
            QueueArmorDraftUpdate(player, slotIndex, std::move(presetLink));
        }
    );
}

PresetApplyDisposition LoadoutManagerSection::ApplyLoadoutPreset(const LoadoutPresetData& data) {
    auto* player = RenderPlayer();
    auto resolution = LoadoutPresetResolver().Resolve(data);
    if (!resolution.success || !resolution.value) {
        AdoptLoadoutDraft(data, true);
        presets.status.SetError("Loaded with unavailable presets: " + resolution.error);
        return PresetApplyDisposition::Applied;
    }
    if (!player) {
        AdoptLoadoutDraft(data, true);
        presets.status.SetInfo("Open a map to equip this loadout.");
        return PresetApplyDisposition::Applied;
    }
    {
        std::lock_guard operationLock(equipmentOperationMutex);
        if (presetSaveState->inProgress.load(std::memory_order_acquire) ||
            loadoutApplyInProgress.load(std::memory_order_acquire) ||
            armorOperationInProgress.load(std::memory_order_acquire)) {
            SetDraftError("Wait for the current equipment change to finish");
            return PresetApplyDisposition::Rejected;
        }
        loadoutApplyInProgress.store(true, std::memory_order_release);
    }
    pendingSlotApply = false;
    const auto generation = loadoutApplyGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
    auto preset = std::move(*resolution.value);
    auto source = data;
    const bool queued = GameHook::QueueAction([this, preset = std::move(preset), source = std::move(source), generation,
                                               expectedPlayer = player](const RuntimeContextSnapshot& runtime) mutable {
        if (runtime.player != expectedPlayer || !runtime.world) {
            FinishLoadoutApply(
                expectedPlayer, generation, std::nullopt,
                runtime.player != expectedPlayer ? LoadoutApplyResult::PlayerChanged : LoadoutApplyResult::Failure
            );
            return;
        }
        std::string error;
        const bool started = EquipmentApplication::ApplyPlayerLoadout(
            runtime.world, expectedPlayer, preset, &error,
            [this, source = std::move(source), generation, expectedPlayer](bool success) mutable {
                FinishLoadoutApply(
                    expectedPlayer, generation,
                    success ? std::optional<LoadoutPresetData>(std::move(source)) : std::nullopt
                );
            }
        );
        if (!started) FinishLoadoutApply(expectedPlayer, generation, std::nullopt);
    });
    if (!queued) FinishLoadoutApply(player, generation, std::nullopt);
    return PresetApplyDisposition::Pending;
}

void LoadoutManagerSection::SetDraftError(std::string error) {
    presets.status.SetError(std::move(error));
    draftStatusToken = presets.status.revision;
}

void LoadoutManagerSection::QueueDraftResult(std::string error) {
    std::lock_guard lock(pendingDraftMutex);
    pendingDraftUpdates.result = std::move(error);
    pendingDraftReady.store(true, std::memory_order_release);
}

void LoadoutManagerSection::QueueWeaponDraftUpdate(
    SDK::AWillie_BP_C* owner, int slotIndex, PresetLink<WeaponPresetData> link
) {
    std::lock_guard lock(pendingDraftMutex);
    if (draftOwner != owner || slotIndex < 0 || slotIndex >= static_cast<int>(pendingDraftUpdates.weapons.size())) return;
    pendingDraftUpdates.weapons[static_cast<std::size_t>(slotIndex)] = std::move(link);
    pendingDraftReady.store(true, std::memory_order_release);
}

void LoadoutManagerSection::QueueArmorDraftUpdate(
    SDK::AWillie_BP_C* owner, int slotIndex, PresetLink<ArmorPresetData> link
) {
    std::lock_guard lock(pendingDraftMutex);
    if (draftOwner != owner || slotIndex < 0 || slotIndex >= static_cast<int>(pendingDraftUpdates.armor.size())) return;
    pendingDraftUpdates.armor[static_cast<std::size_t>(slotIndex)] = std::move(link);
    pendingDraftReady.store(true, std::memory_order_release);
}

void LoadoutManagerSection::QueueClearArmorDraftLinks(SDK::AWillie_BP_C* owner) {
    std::lock_guard lock(pendingDraftMutex);
    if (draftOwner != owner) return;
    pendingDraftUpdates.armor = {};
    pendingDraftUpdates.clearArmorLinks = true;
    pendingDraftReady.store(true, std::memory_order_release);
}

void LoadoutManagerSection::FinishLoadoutApply(
    SDK::AWillie_BP_C* owner, std::uint64_t generation, std::optional<LoadoutPresetData> loadout,
    LoadoutApplyResult failureResult
) {
    std::lock_guard lock(pendingDraftMutex);
    if (loadoutApplyGeneration.load(std::memory_order_relaxed) != generation) return;

    if (draftOwner != owner) {
        loadoutApplyResult.store(LoadoutApplyResult::PlayerChanged, std::memory_order_release);
    } else if (loadout) {
        pendingDraftUpdates.loadout = std::move(*loadout);
        pendingDraftReady.store(true, std::memory_order_release);
        loadoutApplyResult.store(LoadoutApplyResult::Success, std::memory_order_release);
    } else {
        loadoutApplyResult.store(failureResult, std::memory_order_release);
    }
    loadoutApplyInProgress.store(false, std::memory_order_release);
}

void LoadoutManagerSection::AdoptLoadoutDraft(LoadoutPresetData loadout, bool detachedFromRuntime) {
    pendingSlotApply = false;
    const auto& appDataRoot = ConfigManager::GetAppDataPath();
    for (std::size_t slot = 0; slot < weaponSlotLinkStates.size(); ++slot)
        (void)weaponSlotLinkStates[slot].AssignAndResolve(std::move(loadout.weaponSlots[slot]), appDataRoot);
    for (std::size_t slot = 0; slot < armorSlotLinkStates.size(); ++slot) {
        auto& state = armorSlotLinkStates[slot];
        auto resolved = state.AssignAndResolve(std::move(loadout.armorSlots[slot]), appDataRoot);
        if (resolved.success && resolved.value &&
            static_cast<std::size_t>(resolved.value->passport.Slot_30_7561CB484566A4512003EA96ED44F88D) != slot)
            state.MarkBroken("Selected armor belongs to a different slot");
    }
    draftDetachedFromRuntime = detachedFromRuntime;
}

void LoadoutManagerSection::ResetDraftOwner(SDK::AWillie_BP_C* owner) {
    if (draftOwner == owner) return;
    {
        std::lock_guard lock(pendingDraftMutex);
        draftOwner = owner;
        pendingDraftUpdates = {};
        pendingDraftReady.store(false, std::memory_order_relaxed);
        const auto result = loadoutApplyResult.load(std::memory_order_relaxed);
        if (loadoutApplyInProgress.load(std::memory_order_relaxed) || result == LoadoutApplyResult::Success) {
            loadoutApplyGeneration.fetch_add(1, std::memory_order_relaxed);
            loadoutApplyResult.store(LoadoutApplyResult::PlayerChanged, std::memory_order_release);
            loadoutApplyInProgress.store(false, std::memory_order_release);
        }
    }
    if (draftDetachedFromRuntime) return;
    for (auto& state : weaponSlotLinkStates)
        state.Clear();
    for (auto& state : armorSlotLinkStates)
        state.Clear();
    presets.ClearEditing();
}

void LoadoutManagerSection::ConsumePendingDraftUpdates(SDK::AWillie_BP_C* owner) {
    if (!pendingDraftReady.load(std::memory_order_acquire) ||
        !pendingDraftReady.exchange(false, std::memory_order_acq_rel))
        return;
    PendingDraftUpdates updates;
    {
        std::lock_guard lock(pendingDraftMutex);
        updates = std::move(pendingDraftUpdates);
        pendingDraftUpdates = {};
    }

    if (owner == draftOwner) {
        const auto& appDataRoot = ConfigManager::GetAppDataPath();
        const auto assignArmor = [&](std::size_t slot, PresetLink<ArmorPresetData> link) {
            auto& state = armorSlotLinkStates[slot];
            auto resolved = state.AssignAndResolve(std::move(link), appDataRoot);
            if (resolved.success && resolved.value &&
                static_cast<std::size_t>(resolved.value->passport.Slot_30_7561CB484566A4512003EA96ED44F88D) != slot)
                state.MarkBroken("Selected armor belongs to a different slot");
        };
        if (updates.loadout) AdoptLoadoutDraft(std::move(*updates.loadout), false);
        for (std::size_t slot = 0; slot < updates.weapons.size(); ++slot)
            if (updates.weapons[slot])
                (void)weaponSlotLinkStates[slot].AssignAndResolve(std::move(*updates.weapons[slot]), appDataRoot);
        if (updates.clearArmorLinks)
            for (auto& state : armorSlotLinkStates)
                state.Clear();
        for (std::size_t slot = 0; slot < updates.armor.size(); ++slot)
            if (updates.armor[slot]) assignArmor(slot, std::move(*updates.armor[slot]));
    }
    if (updates.result) {
        if (updates.result->empty()) {
            presets.status.ClearText(draftStatusToken);
            draftStatusToken = 0;
        } else {
            SetDraftError(std::move(*updates.result));
        }
    }
}

void LoadoutManagerSection::CheckDraftLinks() {
    const auto& appDataRoot = ConfigManager::GetAppDataPath();
    for (auto& state : weaponSlotLinkStates)
        if (state.HasLink()) (void)state.Resolve(appDataRoot);
    for (std::size_t slot = 0; slot < armorSlotLinkStates.size(); ++slot) {
        auto& state = armorSlotLinkStates[slot];
        if (!state.HasLink()) continue;
        auto resolved = state.Resolve(appDataRoot);
        if (resolved.success && resolved.value &&
            static_cast<std::size_t>(resolved.value->passport.Slot_30_7561CB484566A4512003EA96ED44F88D) != slot)
            state.MarkBroken("Selected armor belongs to a different slot");
    }
}

std::optional<std::string> LoadoutManagerSection::GetBrokenDraftDiagnostic() const {
    for (std::size_t slot = 0; slot < weaponSlotLinkStates.size(); ++slot) {
        const auto& state = weaponSlotLinkStates[slot];
        if (state.IsBroken())
            return "Weapon " + std::string(GetWeaponSlotDisplayName(static_cast<int>(slot))) + ": " +
                   state.GetDiagnostic();
    }
    for (std::size_t slot = 0; slot < armorSlotLinkStates.size(); ++slot) {
        const auto& state = armorSlotLinkStates[slot];
        if (state.IsBroken())
            return "Armor " + std::string(GetArmorSlotDisplayName(static_cast<SDK::EArmorSlots_Enum>(slot))) + ": " +
                   state.GetDiagnostic();
    }
    return std::nullopt;
}

bool LoadoutManagerSection::HasBrokenDraft() const noexcept {
    for (const auto& state : weaponSlotLinkStates)
        if (state.IsBroken()) return true;
    for (const auto& state : armorSlotLinkStates)
        if (state.IsBroken()) return true;
    return false;
}

void LoadoutManagerSection::PresetSaveState::Publish(Completion result) {
    {
        std::lock_guard lock(completionMutex);
        completion = std::move(result);
    }
    completionReady.store(true, std::memory_order_release);
    inProgress.store(false, std::memory_order_release);
}

std::optional<LoadoutManagerSection::PresetSaveState::Completion>
LoadoutManagerSection::PresetSaveState::TakeCompletion() {
    if (!completionReady.load(std::memory_order_acquire) ||
        !completionReady.exchange(false, std::memory_order_acq_rel))
        return std::nullopt;
    std::lock_guard lock(completionMutex);
    auto result = std::move(completion);
    completion.reset();
    return result;
}

bool LoadoutManagerSection::IsEquipmentBusy() const noexcept {
    return presetSaveState->inProgress.load(std::memory_order_acquire) ||
           loadoutApplyInProgress.load(std::memory_order_acquire) ||
           armorOperationInProgress.load(std::memory_order_acquire);
}

PresetBuildResult<LoadoutPresetData> LoadoutManagerSection::QueuePresetSave(std::string name, bool overwrite) {
    std::array<PresetLink<WeaponPresetData>, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weaponLinks;
    for (std::size_t slot = 0; slot < weaponLinks.size(); ++slot)
        weaponLinks[slot] = weaponSlotLinkStates[slot].GetLink();
    std::array<PresetLink<ArmorPresetData>, LoadoutPresetData::K_ARMOR_SLOT_COUNT> armorLinks;
    for (std::size_t slot = 0; slot < armorLinks.size(); ++slot)
        armorLinks[slot] = armorSlotLinkStates[slot].GetLink();

    if (draftDetachedFromRuntime) {
        LoadoutPresetData data;
        data.name = std::move(name);
        data.weaponSlots = std::move(weaponLinks);
        data.armorSlots = std::move(armorLinks);
        return PresetBuildResult<LoadoutPresetData>::Success(std::move(data));
    }

    auto* expectedPlayer = draftOwner;
    if (!expectedPlayer) return PresetBuildResult<LoadoutPresetData>::Failure("A player is required to save a loadout");

    {
        std::lock_guard operationLock(equipmentOperationMutex);
        if (loadoutApplyInProgress.load(std::memory_order_acquire) ||
            armorOperationInProgress.load(std::memory_order_acquire)) {
            return PresetBuildResult<LoadoutPresetData>::Failure("Wait for the current equipment change to finish");
        }
        bool expected = false;
        if (!presetSaveState->inProgress.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            return PresetBuildResult<LoadoutPresetData>::Failure("Wait for the current loadout save to finish");
    }

    auto saveState = presetSaveState;
    const bool queued = GameHook::QueueAction([saveState, name = std::move(name), overwrite,
                                               weaponLinks = std::move(weaponLinks), armorLinks = std::move(armorLinks),
                                               expectedPlayer](const RuntimeContextSnapshot& runtime) mutable {
        auto fail = [state = saveState.get()](std::string error) {
            state->Publish({.operation = {.error = std::move(error)}});
        };
        if (runtime.player != expectedPlayer) {
            fail("The player changed before the loadout could be saved");
            return;
        }
        auto* player = expectedPlayer;

        LoadoutPresetData data;
        data.name = std::move(name);
        data.armorSlots = std::move(armorLinks);
        data.weaponSlots = std::move(weaponLinks);

        auto& armorMap = player->Currently_Equipped_Armor;
        for (auto it = begin(armorMap); it != end(armorMap); ++it) {
            const auto& passport = it->Value();
            if (!IsArmorRemovable(passport)) continue;
            const int slotIndex = static_cast<int>(it->Key());
            if (slotIndex < 0 || slotIndex >= static_cast<int>(data.armorSlots.size())) continue;
            auto& link = data.armorSlots[static_cast<std::size_t>(slotIndex)];
            if (!IsEmptyPresetLink(link)) continue;

            ArmorPresetData snapshot;
            if (!EquipmentApplication::CaptureEquippedArmorPreset(player, it->Key(), snapshot)) {
                fail(
                    "Could not save all armor details for " +
                    std::string(LoadoutManagerSection::GetArmorSlotDisplayName(it->Key()))
                );
                return;
            }
            link = MakePresetCopyLink(std::move(snapshot));
        }

        auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        const std::array<SDK::AActor*, LoadoutPresetData::K_WEAPON_SLOT_COUNT> weaponActors = {
            player->Weapon_R,        player->Weapon_L,        player->Weapon_Slot_R_1,  player->Weapon_Slot_R_2,
            player->Weapon_Slot_L_1, player->Weapon_Slot_L_2, player->Weapon_Slot_Back,
        };
        for (int slotIndex = 0; slotIndex < static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT); ++slotIndex) {
            auto& link = data.weaponSlots[static_cast<std::size_t>(slotIndex)];
            const auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
            if (!IsEmptyPresetLink(link)) continue;
            if (!weaponActors[static_cast<std::size_t>(slotIndex)] &&
                !slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066)
                continue;

            WeaponPresetData snapshot;
            if (!EquipmentApplication::CaptureConfiguredWeaponPreset(player, slotIndex, snapshot)) {
                fail(
                    "Could not save all weapon details for " +
                    std::string(LoadoutManagerSection::GetWeaponSlotDisplayName(slotIndex))
                );
                return;
            }
            link = MakePresetCopyLink(std::move(snapshot));
        }

        saveState->Publish({.data = std::move(data), .overwrite = overwrite});
    });
    if (!queued) saveState->Publish({.operation = {.error = "Could not start the loadout save"}});
    return PresetBuildResult<LoadoutPresetData>::Pending();
}

void LoadoutManagerSection::ConsumePresetSaveCompletion() {
    auto completion = presetSaveState->TakeCompletion();
    if (!completion) return;

    auto result = std::move(completion->operation);
    if (completion->data) {
        auto& data = *completion->data;
        result = LoadoutPresetSerializer::SavePresetByNameResult(data.name, data, completion->overwrite);
    }
    presets.CompletePendingSave(std::move(result));
}

void LoadoutManagerSection::RenderArmorTab() {
    ImGui::PushID("armor");
    auto* player = RenderPlayer();

    if (!player) {
        ImGui::TextDisabled("No player is active");
        ImGui::PopID();
        return;
    }

    GuiUtils::RenderFreeTierCombo("Armor Tier", cfg.generateTier);
    ArmorGenerationUi::RenderOptions(cfg.armorOptions);

    ImGui::Spacing();
    ImGui::PushID("actions");
    if (GuiUtils::Button("Remove Removable Armor", GuiUtils::ButtonTone::Danger))
        ImGui::OpenPopup("Remove Removable Armor");
    (void)GuiUtils::SameLineIfFitsButton("Randomize All Armor");
    if (GuiUtils::Button("Randomize All Armor", GuiUtils::ButtonTone::Primary)) RandomizeAllArmor();

    if (ImGui::BeginPopupModal("Remove Removable Armor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Remove all armor that can be unequipped?");
        ImGui::Spacing();
        if (GuiUtils::Button("Remove Armor", GuiUtils::ButtonTone::Danger)) {
            StripAllArmor();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopID();

    (void)armorPresetComposer.Render("Armor Preset##ap");
    if (armorPresetComposer.HasLink()) {
        if (GuiUtils::Button("Add Selected Armor")) ImportArmorPreset();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            ImGui::SetItemTooltip("Use the selected armor in its saved slot");
    }

    ImGui::Separator();

    auto& armorMap = player->Currently_Equipped_Armor;

    if (armorMap.Num() == 0) {
        ImGui::TextDisabled("No armor equipped. Choose a preset above to add armor.");
        ImGui::PopID();
        return;
    }

    for (auto it = begin(armorMap); it != end(armorMap); ++it) {
        auto slotEnum = it->Key();
        auto& passport = it->Value();
        const char* slotName = GetArmorSlotDisplayName(slotEnum);
        const int slotIndex = static_cast<int>(slotEnum);

        ImGui::PushID(slotIndex);
        if (slotIndex < 0 || slotIndex >= static_cast<int>(armorSlotLinkStates.size())) {
            ImGui::TextDisabled("%s - unsupported slot", slotName);
            ImGui::PopID();
            continue;
        }

        bool hasArmor = passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43 != nullptr;
        const bool removable = !hasArmor || IsArmorRemovable(passport);
        const char* className =
            armorNameCache[static_cast<std::size_t>(slotIndex)].Get(
                passport.ArmorCore_3_F6B7C69C4BD7D9720DB91EB635EE2B43
            );
        bool open = ImGui::TreeNodeEx(slotName, ImGuiTreeNodeFlags_DefaultOpen, "%s - %s", slotName, className);

        if (open) {
            if (!removable) ImGui::TextDisabled("This armor is part of the character and cannot be removed");
            auto& linkState = armorSlotLinkStates[static_cast<std::size_t>(slotIndex)];
            const bool linked = linkState.HasLink();
            if (linked) {
                const bool copied = GetPresetCopy(linkState.GetLink()) != nullptr;
                RenderPresetComponentBadge(copied, linkState.IsBroken(), linkState.GetDiagnostic());
                (void)GuiUtils::SameLineIfFitsButton("Edit Independently");
                if (ImGui::SmallButton("Edit Independently")) linkState.Clear();
                RenderPresetComponentDetail(linkState.IsBroken(), linkState.GetDiagnostic());
            }
            if (linked) ImGui::BeginDisabled();
            if (hasArmor) {
                std::optional<SDK::FLinearColor> editedColor1;
                std::optional<SDK::FLinearColor> editedColor2;
                float col1[4] =
                    {passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.R,
                     passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.G,
                     passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.B,
                     passport.FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393.A};
                float col2[4] =
                    {passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.R,
                     passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.G,
                     passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.B,
                     passport.FabricColor2_17_4199336A482894E5BC99E69E52B50B1C.A};

                GuiUtils::SetNextColorFieldWidth("Fabric Color 1##c1");
                if (ImGui::ColorEdit4("Fabric Color 1##c1", col1, ImGuiColorEditFlags_AlphaBar)) {
                    editedColor1 = SDK::FLinearColor{col1[0], col1[1], col1[2], col1[3]};
                }
                GuiUtils::SetNextColorFieldWidth("Fabric Color 2##c2");
                if (ImGui::ColorEdit4("Fabric Color 2##c2", col2, ImGuiColorEditFlags_AlphaBar)) {
                    editedColor2 = SDK::FLinearColor{col2[0], col2[1], col2[2], col2[3]};
                }

                if (editedColor1 || editedColor2) {
                    GameHook::QueueAction([slotEnum, editedColor1,
                                           editedColor2](const RuntimeContextSnapshot& runtime) {
                        if (!runtime.player) return;
                        auto& currentArmor = runtime.player->Currently_Equipped_Armor;
                        for (auto current = begin(currentArmor); current != end(currentArmor); ++current) {
                            if (current->Key() != slotEnum) continue;
                            if (editedColor1)
                                current->Value().FabricColor1_15_4C7C24744C4F50FFAFB62DB50DE29393 = *editedColor1;
                            if (editedColor2)
                                current->Value().FabricColor2_17_4199336A482894E5BC99E69E52B50B1C = *editedColor2;
                            break;
                        }
                    });
                    if (cfg.livePreview && removable) {
                        pendingSlot = slotEnum;
                        pendingSlotApply = true;
                    }
                }

                if (!removable) ImGui::BeginDisabled();
                if (ImGui::Button("Remove Armor")) RemoveArmorForSlot(slotEnum);
                (void)GuiUtils::SameLineIfFitsButton("New Random Armor");
                if (ImGui::Button("New Random Armor")) {
                    GenerateArmorForSlot(slotEnum);
                }
                if (!removable) ImGui::EndDisabled();
            } else {
                if (ImGui::Button("Add Random Armor")) {
                    GenerateArmorForSlot(slotEnum);
                }
            }
            if (linked) ImGui::EndDisabled();

            if (armorPresetComposer.HasLink()) {
                if (!removable) ImGui::BeginDisabled();
                if (ImGui::Button(linked ? "Replace with Selected Preset" : "Use Selected Preset"))
                    ImportArmorPreset(slotEnum);
                if (!removable) ImGui::EndDisabled();
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::PopID();
}

void LoadoutManagerSection::RenderWeaponSlotModules(int slotIndex, const SDK::FStr_WeaponParts& slot) {
    if (!modulePool.populated.load(std::memory_order_acquire)) {
        ImGui::TextDisabled("Loading weapon parts...");
        return;
    }

    auto& modules = modulePool.all;
    auto* head = slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F;
    auto* guard = slot.GuardModule_21_774015784EB0300D2671C894D57ED144;
    auto* grip = slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867;
    auto* pommel = slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984;
    auto* subModule1 = slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0;
    auto* subModule2 = slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980;
    GuiUtils::RenderGlobalModuleCombo("Head", head, modules.heads, moduleFilters[0], modules.cachedWidths[0]);
    GuiUtils::RenderGlobalModuleCombo("Guard", guard, modules.guards, moduleFilters[1], modules.cachedWidths[1]);
    GuiUtils::RenderGlobalModuleCombo("Grip", grip, modules.grips, moduleFilters[2], modules.cachedWidths[2]);
    GuiUtils::RenderGlobalModuleCombo("Pommel", pommel, modules.pommels, moduleFilters[3], modules.cachedWidths[3]);
    if (!modules.subMods1.empty())
        GuiUtils::RenderGlobalModuleCombo(
            "Extra Head Part 1", subModule1, modules.subMods1, moduleFilters[4], modules.cachedWidths[4]
        );
    if (!modules.subMods2.empty())
        GuiUtils::RenderGlobalModuleCombo(
            "Extra Head Part 2", subModule2, modules.subMods2, moduleFilters[5], modules.cachedWidths[5]
        );

    if (head == slot.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F &&
        guard == slot.GuardModule_21_774015784EB0300D2671C894D57ED144 &&
        grip == slot.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 &&
        pommel == slot.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 &&
        subModule1 == slot.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 &&
        subModule2 == slot.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980)
        return;

    GameHook::QueueAction([slotIndex, head, guard, grip, pommel, subModule1,
                           subModule2](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player) return;
        auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& current = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
        current.HeadModule_19_B043442745EED9AD1BE7929F0A06DB8F = head;
        current.GuardModule_21_774015784EB0300D2671C894D57ED144 = guard;
        current.GripModule_38_15B14C3F4E9701389A9B35A3B0909867 = grip;
        current.PommelModule_22_4F6D0ABC4AA88CF780EE1C9649F96984 = pommel;
        current.HeadSubModule1_66_EA08538346D6DADCE01E8B8B7B50A9A0 = subModule1;
        current.HeadSubModule2_67_491313E24CE70DD60B5A6D88ED4B5980 = subModule2;
    });
}

void LoadoutManagerSection::RenderWeaponSlotSizes(int slotIndex, const SDK::FStr_WeaponParts& slot) {
    ImGui::TextDisabled("Sizes");
    auto head = slot.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA;
    auto guard = slot.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D;
    auto pommel = slot.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524;
    const bool edited = RenderVectorDrag("Head##sz", head) | RenderVectorDrag("Guard##sz", guard) |
                        RenderVectorDrag("Pommel##sz", pommel);
    if (!edited) return;

    GameHook::QueueAction([slotIndex, head, guard, pommel](const RuntimeContextSnapshot& runtime) {
        if (!runtime.player) return;
        auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
        auto& current = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
        current.HeadSize_23_5DF30AE0493E534BD92D5B95E31E13CA = head;
        current.GuardSize_24_7EB9BB3F4B7B54DD51CE529FEEA9A98D = guard;
        current.PommelPommelSize_26_5B37388746A83FCB7A7833891C1C5524 = pommel;
    });
}

void LoadoutManagerSection::RenderWeaponSlotMaterials(int slotIndex, const SDK::FStr_WeaponParts& slot) {
    const auto& matMap = slot.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81;
    if (matMap.Num() == 0) return;

    ImGui::TextDisabled("Materials");
    for (auto it = begin(matMap); it != end(matMap); ++it) {
        const char* label;
        switch (it->Key()) {
            case SDK::Enum_Weapon_Material_Type::NewEnumerator0: label = "Steel##mat"; break;
            case SDK::Enum_Weapon_Material_Type::NewEnumerator1: label = "Colored Metal##mat"; break;
            case SDK::Enum_Weapon_Material_Type::NewEnumerator2: label = "Wood##mat"; break;
            case SDK::Enum_Weapon_Material_Type::NewEnumerator3: label = "Leather##mat"; break;
            default: continue;
        }
        const auto key = it->Key();
        auto material = it->Value();
        GuiUtils::RenderMaterialCombo(label, material);
        if (material != it->Value()) {
            GameHook::QueueAction([slotIndex, key, material](const RuntimeContextSnapshot& runtime) {
                if (!runtime.player) return;
                auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                auto& current = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
                auto& materials = current.MemberVar_40_43_0B501E224AC2292FC03A999C237C2C81;
                for (auto entry = begin(materials); entry != end(materials); ++entry) {
                    if (entry->Key() == key) {
                        entry->Value() = material;
                        break;
                    }
                }
            });
        }
    }

    const auto& colorMap = slot.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0;
    for (auto it = begin(colorMap); it != end(colorMap); ++it) {
        const char* label;
        switch (it->Key()) {
            case SDK::Enum_Weapon_Material_Type::NewEnumerator2: label = "Wood Color##col"; break;
            case SDK::Enum_Weapon_Material_Type::NewEnumerator3: label = "Leather Color##col"; break;
            default: continue;
        }
        float col[4] = {it->Value().R, it->Value().G, it->Value().B, it->Value().A};
        GuiUtils::SetNextColorFieldWidth(label);
        if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaBar)) {
            const auto key = it->Key();
            const SDK::FLinearColor color{col[0], col[1], col[2], col[3]};
            GameHook::QueueAction([slotIndex, key, color](const RuntimeContextSnapshot& runtime) {
                if (!runtime.player) return;
                auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                auto& current = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
                auto& colors = current.MemberVar_44_45_FF627FBE4FE882E7D295BFA0BB6716C0;
                for (auto entry = begin(colors); entry != end(colors); ++entry) {
                    if (entry->Key() == key) {
                        entry->Value() = color;
                        break;
                    }
                }
            });
        }
    }
}

void LoadoutManagerSection::RenderWeaponsTab() {
    ImGui::PushID("weapons");
    auto* player = RenderPlayer();

    if (!player) {
        ImGui::TextDisabled("No player is active");
        ImGui::PopID();
        return;
    }

    EnsureModulePool();

    GuiUtils::RenderFreeTierCombo("Weapon Tier", cfg.generateTier);
    GuiUtils::HelpTooltip("Quality of new random weapons");

    if (WeaponGenerationUi::RenderSpecificTypeCombo("Weapon Type", cfg.weaponSpecificType, true)) {
        ConfigManager::Get()
            .SetInt(LOADOUT_CONFIG_SECTION, WeaponGenerationUi::SPECIFIC_TYPE_CONFIG_KEY, cfg.weaponSpecificType);
    }
    GuiUtils::HelpTooltip("Choose the kind of random weapon you want");
    ImGui::Spacing();

    if (GuiUtils::Button("Clear All Weapons", GuiUtils::ButtonTone::Danger)) ImGui::OpenPopup("Clear All Weapons");

    if (ImGui::BeginPopupModal("Clear All Weapons", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Remove every equipped weapon?");
        ImGui::Spacing();
        if (GuiUtils::Button("Clear Weapons", GuiUtils::ButtonTone::Danger)) {
            ClearAllWeapons();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (GuiUtils::Button("Cancel", GuiUtils::ButtonTone::Quiet)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    (void)weaponPresetComposer.Render("Weapon Preset##wp");

    ImGui::Separator();

    auto& weapons = player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;

    for (int i = 0; i < static_cast<int>(LoadoutPresetData::K_WEAPON_SLOT_COUNT); ++i) {
        auto& slot = LoadoutPresetData::GetWeaponSlot(weapons, i);
        bool hasWeapon = slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066 != nullptr;
        const char* className = weaponNameCache[i].Get(slot.WeaponBPClass_51_5C40F9BE43F7897FB12AACA75C2AD066);

        ImGui::PushID(i);

        const char* slotName = GetWeaponSlotDisplayName(i);
        bool open = ImGui::TreeNodeEx(slotName, 0, "%s - %s", slotName, className);

        if (open) {
            auto& linkState = weaponSlotLinkStates[static_cast<std::size_t>(i)];
            const bool linked = linkState.HasLink();
            if (linked) {
                const bool copied = GetPresetCopy(linkState.GetLink()) != nullptr;
                RenderPresetComponentBadge(copied, linkState.IsBroken(), linkState.GetDiagnostic());
                (void)GuiUtils::SameLineIfFitsButton("Edit Independently");
                if (ImGui::SmallButton("Edit Independently")) linkState.Clear();
                RenderPresetComponentDetail(linkState.IsBroken(), linkState.GetDiagnostic());
            }
            if (linked) ImGui::BeginDisabled();
            if (hasWeapon) {
                RenderWeaponSlotModules(i, slot);

                ImGui::Spacing();
                RenderWeaponSlotSizes(i, slot);

                ImGui::Spacing();
                RenderWeaponSlotMaterials(i, slot);

                ImGui::Spacing();
                ImGui::SetNextItemWidth(GuiUtils::K_DRAG_WIDTH);
                auto coa = slot.COAInt_63_593665BE4EF020F95F7D1A92564C1239;
                ImGui::InputInt("Coat of Arms##coa", &coa);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    GameHook::QueueAction([slotIndex = i, coa](const RuntimeContextSnapshot& runtime) {
                        if (!runtime.player) return;
                        auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                        LoadoutPresetData::GetWeaponSlot(weapons, slotIndex)
                            .COAInt_63_593665BE4EF020F95F7D1A92564C1239 = coa;
                    });
                }

                ImGui::Spacing();
                if (GuiUtils::Button("Update Equipped Weapon", GuiUtils::ButtonTone::Primary)) ApplyWeaponToPlayer(i);
                (void)GuiUtils::SameLineIfFitsButton("Remove Weapon");
                if (GuiUtils::Button("Remove Weapon", GuiUtils::ButtonTone::Danger)) {
                    GameHook::QueueAction([this, slotIndex = i](const RuntimeContextSnapshot& runtime) {
                        if (!runtime.player || !runtime.world) return;
                        auto& weapons = runtime.player->Load_Equipment.Weapons_83_06F076E247B54D0D9942B383323C1968;
                        auto& current = LoadoutPresetData::GetWeaponSlot(weapons, slotIndex);
                        const WeaponSlotSnapshot previousSlot(current);
                        EquipmentApplication::ClearWeaponSlot(current);
                        std::string applyError;
                        if (!EquipmentApplication::SynchronizeConfiguredWeaponActors(
                                runtime.world, runtime.player, slotIndex, nullptr, &applyError
                            )) {
                            previousSlot.Restore(current);
                            QueueDraftResult(
                                "Could not remove weapon from " +
                                    std::string(GetWeaponSlotDisplayName(slotIndex)) + ": " +
                                    applyError
                            );
                            return;
                        }
                        QueueWeaponDraftUpdate(runtime.player, slotIndex, {});
                        QueueDraftResult();
                    });
                }
            } else {
                ImGui::TextDisabled("(empty slot)");
            }
            if (linked) ImGui::EndDisabled();

            const char* generateLabel =
                linked ? "Replace with Random Weapon" : (hasWeapon ? "New Random Weapon" : "Add Random Weapon");
            if (GuiUtils::Button(generateLabel)) GenerateWeaponForSlot(i);

            if (weaponPresetComposer.HasLink()) {
                const char* presetLabel = linked ? "Replace with Selected Preset" : "Use Selected Preset";
                (void)GuiUtils::SameLineIfFitsButton(presetLabel);
                if (GuiUtils::Button(presetLabel)) ImportWeaponPreset(i);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::PopID();
}

LoadoutManagerSection::LoadoutManagerSection(ModContext& ctx) : Section(ctx, SECTION) {
    InitKeybinds();
    cfg.weaponSpecificType = ConfigManager::Get().GetInt(
        LOADOUT_CONFIG_SECTION, WeaponGenerationUi::SPECIFIC_TYPE_CONFIG_KEY, cfg.weaponSpecificType
    );
    keybindConfigSnapshot = {.tier = cfg.generateTier, .options = cfg.armorOptions};
}

void LoadoutManagerSection::InitKeybinds() {
    keybinds.Add({
        .name = "Refresh Equipped Armor",
        .tooltip = "Refresh the appearance and fit of your equipped armor",
        .configSection = "ApplyLoadout",
        .keyPtr = &cfg.applyKey,
        .callback = [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) {
            QueueArmorTransaction(
                "Refresh equipped armor",
                [](const RuntimeContextSnapshot&, std::vector<ArmorPresetData>&, std::string&) { return true; },
                nullptr, &runtime
            );
        },
    });

    keybinds.Add({
        .name = "Random Armor Loadout",
        .tooltip = "Equip a new random armor set",
        .configSection = "RandomizeEquipment",
        .keyPtr = &cfg.randomizeKey,
        .callback =
            [this]([[maybe_unused]] bool, const RuntimeContextSnapshot& runtime) { RandomizeAllArmor(&runtime); },
        .params =
            {KeybindParam("live_preview", "Preview Changes", &cfg.livePreview, "Show armor edits on the player"),
             KeybindParam("tier", "Armor Tier", &cfg.generateTier, 0, 8, "Tier for the new armor")},
    });
}

void LoadoutManagerSection::Render() {
    ConsumePresetSaveCompletion();
    const auto& options = cfg.armorOptions;
    const auto& publishedOptions = keybindConfigSnapshot.options;
    if (keybindConfigSnapshot.tier != cfg.generateTier || publishedOptions.moduleChance != options.moduleChance ||
        publishedOptions.forceMetalMaterial != options.forceMetalMaterial ||
        publishedOptions.steelType != options.steelType || publishedOptions.metalPiecesType != options.metalPiecesType) {
        std::lock_guard lock(keybindConfigMutex);
        keybindConfigSnapshot = {.tier = cfg.generateTier, .options = options};
    }
    auto* player = RenderPlayer();
    ResetDraftOwner(player);
    auto applyResult = loadoutApplyResult.load(std::memory_order_acquire);
    if (applyResult != LoadoutApplyResult::None)
        applyResult = loadoutApplyResult.exchange(LoadoutApplyResult::None, std::memory_order_acq_rel);
    ConsumePendingDraftUpdates(player);

    switch (applyResult) {
        case LoadoutApplyResult::Success:
            if (presets.IsApplyPending()) presets.CompletePendingApply(true);
            break;
        case LoadoutApplyResult::Failure:
            if (presets.IsApplyPending())
                presets.CompletePendingApply(false, "Loadout could not be equipped");
            else
                presets.status.SetError("Loadout could not be equipped");
            break;
        case LoadoutApplyResult::PlayerChanged:
            if (presets.IsApplyPending())
                presets.CompletePendingApply(false, "The player changed before the loadout could be equipped");
            else
                presets.status.SetError("The player changed before the loadout could be equipped");
            break;
        case LoadoutApplyResult::None: break;
    }
    const bool equipmentBusy = IsEquipmentBusy();

    keybinds.Render();
    ImGui::Spacing();
    presets.status.Render();

    const bool hasPresetLinks = [&] {
        for (const auto& state : weaponSlotLinkStates)
            if (state.HasLink()) return true;
        for (const auto& state : armorSlotLinkStates)
            if (state.HasLink()) return true;
        return false;
    }();
    if (hasPresetLinks) {
        if (ImGui::Button("Check Preset Availability")) {
            CheckDraftLinks();
            if (const auto broken = GetBrokenDraftDiagnostic())
                presets.status.SetError("Unavailable loadout preset: " + *broken);
            else
                presets.status.Notify("All loadout presets are available");
        }
        presets.status.RenderResult();
        ImGui::TextDisabled("Unavailable presets must be fixed before saving");
    }
    const bool brokenDraft = HasBrokenDraft();
    if (draftDetachedFromRuntime) {
        ImGui::Spacing();
        ImGui::TextWrapped("This preset is being edited independently. Empty slots stay empty when saved.");
        if (!player) ImGui::BeginDisabled();
        if (ImGui::Button("Use Current Equipment")) {
            draftDetachedFromRuntime = false;
            presets.status.Clear();
        }
        if (!player) ImGui::EndDisabled();
    }
    if (brokenDraft) {
        if (draftDetachedFromRuntime) (void)GuiUtils::SameLineIfFitsButton("Remove Unavailable Presets");
        if (ImGui::Button("Remove Unavailable Presets")) {
            for (auto& state : weaponSlotLinkStates)
                if (state.IsBroken()) state.Clear();
            for (auto& state : armorSlotLinkStates)
                if (state.IsBroken()) state.Clear();
            presets.status.Clear();
        }
    }

    if (!equipmentBusy && cfg.livePreview && pendingSlotApply) {
        static constexpr double APPLY_COOLDOWN = 0.3;
        if (ImGui::GetTime() - lastSlotApplyTime >= APPLY_COOLDOWN) {
            ReapplyArmorSlot(pendingSlot);
            lastSlotApplyTime = ImGui::GetTime();
            pendingSlotApply = false;
        }
    }

    ImGui::Spacing();
    static constexpr const char* EQ_TAB_LABELS[] = {"Armor", "Weapons", "Presets"};
    GuiUtils::RenderUnderlineTabs("##EquipmentTabs", activeTab, EQ_TAB_LABELS, 3);
    switch (activeTab) {
        case 0:
            if (equipmentBusy || draftDetachedFromRuntime) ImGui::BeginDisabled();
            RenderArmorTab();
            if (equipmentBusy || draftDetachedFromRuntime) ImGui::EndDisabled();
            break;
        case 1:
            if (equipmentBusy || draftDetachedFromRuntime) ImGui::BeginDisabled();
            RenderWeaponsTab();
            if (equipmentBusy || draftDetachedFromRuntime) ImGui::EndDisabled();
            break;
        case 2:
            presets.RenderPresetsTab(
                [this](const char* name, bool overwrite) { return QueuePresetSave(name, overwrite); },
                [this](const LoadoutPresetData& d) { return ApplyLoadoutPreset(d); },
                player != nullptr && !equipmentBusy, "Equip"
            );
            break;
        default: break;
    }
}
