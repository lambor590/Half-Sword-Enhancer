#include "Utils/BoneControl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "SDK/Engine_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/PossessState.h"

namespace BoneControl {
    namespace {
        constexpr double NO_DISLOCATION_STRENGTH_MULTIPLIER = 1'000'000.0;
        constexpr float MASS_SCALE_EPSILON = 0.001f;

        struct ConstraintBaseline {
            SDK::UPhysicsConstraintComponent* component = nullptr;
            bool linearBreakable = false;
            float linearThreshold = 0.0f;
            bool angularBreakable = false;
            float angularThreshold = 0.0f;
        };

        using ThresholdBaseline = std::vector<std::pair<SDK::FName, double>>;

        struct WillieBaseline {
            int objectIndex = -1;
            ThresholdBaseline linear;
            ThresholdBaseline angular;
            std::vector<ConstraintBaseline> constraints;
            double strength = -1.0;
            float mass = -1.0f;
        };

        struct PendingSpawnMass {
            SDK::AWillie_BP_C* willie = nullptr;
            int objectIndex = -1;
            bool sawConfiguredMass = false;
        };

        std::vector<std::pair<SDK::AWillie_BP_C*, WillieBaseline>> g_baselines;
        std::vector<PendingSpawnMass> g_pendingSpawnMass;

        WillieBaseline& BaselineFor(SDK::AWillie_BP_C* willie) {
            for (auto& [storedWillie, baseline] : g_baselines) {
                if (storedWillie == willie) return baseline;
            }
            return g_baselines.emplace_back(willie, WillieBaseline{}).second;
        }

        template <typename Fn>
        void ForEachEnemy(const RuntimeContextSnapshot& runtime, Fn&& fn) {
            if (!runtime.world || !runtime.player) return;
            ActorUtils::ForEachWillie(runtime.world, runtime.player, [&](SDK::AWillie_BP_C* willie) {
                if (MatchesScope(willie, false)) fn(willie);
            });
        }

        template <typename Fn>
        void ForEachDislocationConstraint(SDK::AWillie_BP_C* willie, Fn&& fn) {
            for (auto* component :
                 {willie->Dislocated_Bone_Constraint_Arm_R, willie->Dislocated_Bone_Constraint_Arm_L,
                  willie->Dislocated_Bone_Constraint_Leg_R, willie->Dislocated_Bone_Constraint_Leg_L,
                  willie->Dislocated_Bone_Constraint_Neck, willie->Dislocated_Bone_Constraint_Back}) {
                if (component) fn(component);
            }
        }

        template <typename Fn>
        void ForEachBoneMesh(SDK::AWillie_BP_C* willie, Fn&& fn) {
            if (willie->Mesh) fn(willie->Mesh);
            if (willie->BoneCore && willie->BoneCore != willie->Mesh) fn(willie->BoneCore);
        }

        PendingSpawnMass* FindPendingSpawnMass(SDK::AWillie_BP_C* willie) {
            if (!willie) return nullptr;
            for (auto& pending : g_pendingSpawnMass) {
                if (pending.willie == willie && pending.objectIndex == willie->Index) return &pending;
            }
            return nullptr;
        }

        void RemovePendingSpawnMass(SDK::AWillie_BP_C* willie, int objectIndex) {
            std::erase_if(g_pendingSpawnMass, [willie, objectIndex](const PendingSpawnMass& pending) {
                return pending.willie == willie && pending.objectIndex == objectIndex;
            });
        }

        double BaselineValueFor(const ThresholdBaseline& baseline, const SDK::FName& bone, double fallback) noexcept {
            for (const auto& entry : baseline) {
                if (entry.first == bone) return entry.second;
            }
            return fallback;
        }

        bool CaptureThresholds(SDK::TMap<SDK::FName, double>& map, ThresholdBaseline& baseline) {
            bool changed = false;
            for (auto& entry : map) {
                const auto bone = entry.Key();
                bool known = false;
                for (const auto& baselineEntry : baseline) {
                    if (baselineEntry.first == bone) {
                        known = true;
                        break;
                    }
                }
                if (!known) {
                    baseline.emplace_back(bone, entry.Value());
                    changed = true;
                }
            }
            return changed;
        }

        ConstraintBaseline& CaptureConstraint(
            SDK::UPhysicsConstraintComponent* component, std::vector<ConstraintBaseline>& baseline
        ) {
            for (auto& item : baseline) {
                if (item.component == component) return item;
            }

            auto& item = baseline.emplace_back();
            item.component = component;
            auto accessor = component->GetConstraint();
            SDK::UConstraintInstanceBlueprintLibrary::GetLinearBreakable(
                accessor, &item.linearBreakable, &item.linearThreshold
            );
            SDK::UConstraintInstanceBlueprintLibrary::GetAngularBreakable(
                accessor, &item.angularBreakable, &item.angularThreshold
            );
            return item;
        }

        void ApplyDislocationConstraints(
            SDK::AWillie_BP_C* willie, std::vector<ConstraintBaseline>& baseline, bool blockBreak
        ) {
            ForEachDislocationConstraint(willie, [&](SDK::UPhysicsConstraintComponent* component) {
                const auto& original = CaptureConstraint(component, baseline);
                if (blockBreak) {
                    component->SetLinearBreakable(false, (std::numeric_limits<float>::max)());
                    component->SetAngularBreakable(false, (std::numeric_limits<float>::max)());
                } else {
                    component->SetLinearBreakable(original.linearBreakable, original.linearThreshold);
                    component->SetAngularBreakable(original.angularBreakable, original.angularThreshold);
                }
            });
        }

        void ApplyThresholds(
            SDK::AWillie_BP_C* willie, SDK::TMap<SDK::FName, double>& map, const ThresholdBaseline& baseline,
            double multiplier, bool linear, bool breakable
        ) {
            for (auto& entry : map) {
                const double scaled = BaselineValueFor(baseline, entry.Key(), entry.Value()) * multiplier;
                entry.Value() = scaled;
                if (entry.Key().IsNone()) continue;

                const auto threshold = static_cast<float>(
                    std::clamp(scaled, 0.0, static_cast<double>((std::numeric_limits<float>::max)()))
                );
                ForEachBoneMesh(willie, [&](SDK::USkeletalMeshComponent* mesh) {
                    SDK::TArray<SDK::FConstraintInstanceAccessor> constraints;
                    mesh->GetConstraintsFromBody(entry.Key(), true, true, false, &constraints);
                    for (auto& constraint : constraints) {
                        if (linear) {
                            SDK::UConstraintInstanceBlueprintLibrary::SetLinearBreakable(
                                constraint, breakable, threshold
                            );
                        } else {
                            SDK::UConstraintInstanceBlueprintLibrary::SetAngularBreakable(
                                constraint, breakable, threshold
                            );
                        }
                    }
                });
            }
        }

        template <typename Fn>
        void ForEachMassScaleBone(SDK::AWillie_BP_C* willie, Fn&& fn) {
            for (const auto* bones :
                 {&willie->Rigid_Bones, &willie->Upper_Torso_Bones, &willie->Lower_Torso_Bones, &willie->R_Arm_Bones,
                  &willie->L_Arm_Bones, &willie->R_Leg_Bones, &willie->L_Leg_Bones, &willie->Neck_Bones,
                  &willie->Head_Bones, &willie->Breakable_Bones_List}) {
                for (const auto& bone : *bones) {
                    if (!bone.IsNone()) fn(bone);
                }
            }
        }

        void ApplyMassScale(SDK::AWillie_BP_C* willie, float multiplier) {
            ForEachBoneMesh(willie, [&](SDK::USkeletalMeshComponent* mesh) {
                mesh->SetAllMassScale(multiplier);
                ForEachMassScaleBone(willie, [&](const SDK::FName& bone) { mesh->SetMassScale(bone, multiplier); });
                for (auto* limits :
                     {&willie->Bones_Linear_Breakable_Limits, &willie->Bones_Angular_Breakable_Limits}) {
                    for (auto& entry : *limits) {
                        if (!entry.Key().IsNone()) mesh->SetMassScale(entry.Key(), multiplier);
                    }
                }
            });
        }

        bool HasMassScale(SDK::AWillie_BP_C* willie, float multiplier) {
            bool checkedBone = false;
            bool matched = true;
            ForEachBoneMesh(willie, [&](SDK::USkeletalMeshComponent* mesh) {
                if (!matched) return;
                ForEachMassScaleBone(willie, [&](const SDK::FName& bone) {
                    if (!matched) return;
                    checkedBone = true;
                    matched = std::abs(mesh->GetMassScale(bone) - multiplier) <= MASS_SCALE_EPSILON;
                });
            });
            return checkedBone && matched;
        }
    }

    SDK::AWillie_BP_C* WillieOwner(SDK::UObject* object) {
        if (!object) return nullptr;
        if (object->IsA(SDK::AWillie_BP_C::StaticClass())) return static_cast<SDK::AWillie_BP_C*>(object);
        if (!object->IsA(SDK::UActorComponent::StaticClass())) return nullptr;

        auto* owner = static_cast<SDK::UActorComponent*>(object)->GetOwner();
        return owner && owner->IsA(SDK::AWillie_BP_C::StaticClass()) ? static_cast<SDK::AWillie_BP_C*>(owner)
                                                                     : nullptr;
    }

    bool MatchesScope(SDK::AWillie_BP_C* willie, bool playerScope) noexcept {
        if (!willie) return false;
        const auto& snapshot = ModContext::Get().GetRenderSnapshot();
        const bool isPlayer = willie == snapshot.player || willie->Player;
        return playerScope ? isPlayer : !isPlayer && willie != PossessState::GetOriginalPawn();
    }

    bool ShouldCancelBreak(GameHook::ProcessEventContext& context, SDK::AWillie_BP_C* willie) {
        if (!context.object->IsA(SDK::UPhysicsConstraintComponent::StaticClass())) return true;

        const auto* component = static_cast<SDK::UPhysicsConstraintComponent*>(context.object);
        return component == willie->Dislocated_Bone_Constraint_Arm_R ||
               component == willie->Dislocated_Bone_Constraint_Arm_L ||
               component == willie->Dislocated_Bone_Constraint_Leg_R ||
               component == willie->Dislocated_Bone_Constraint_Leg_L ||
               component == willie->Dislocated_Bone_Constraint_Neck ||
               component == willie->Dislocated_Bone_Constraint_Back;
    }

    void MarkSpawnedWillie(SDK::AWillie_BP_C* willie) {
        if (!willie || FindPendingSpawnMass(willie)) return;
        g_pendingSpawnMass.push_back({.willie = willie, .objectIndex = willie->Index});
    }

    void ClearPendingSpawnMass(bool playerScope) {
        std::erase_if(g_pendingSpawnMass, [playerScope](const PendingSpawnMass& pending) {
            return !pending.willie || pending.willie->Index != pending.objectIndex ||
                   MatchesScope(pending.willie, playerScope);
        });
    }

    void ApplyPendingSpawnMass(SDK::AWillie_BP_C* willie, const Settings& settings) {
        auto* pending = FindPendingSpawnMass(willie);
        if (!pending) return;

        if (settings.massMultiplier == 1.0f) {
            RemovePendingSpawnMass(willie, pending->objectIndex);
            return;
        }

        if (HasMassScale(willie, settings.massMultiplier)) {
            pending->sawConfiguredMass = true;
            return;
        }

        Apply(willie, settings, true);
        if (pending->sawConfiguredMass) {
            RemovePendingSpawnMass(willie, pending->objectIndex);
        } else {
            pending->sawConfiguredMass = true;
        }
    }

    void Apply(SDK::AWillie_BP_C* willie, const Settings& settings, bool force) {
        if (!willie) return;

        auto& baseline = BaselineFor(willie);
        if (baseline.objectIndex != willie->Index) {
            baseline = WillieBaseline{.objectIndex = willie->Index};
        }
        force |= CaptureThresholds(willie->Bones_Linear_Breakable_Limits, baseline.linear);
        force |= CaptureThresholds(willie->Bones_Angular_Breakable_Limits, baseline.angular);

        const bool blockBreak = settings.blockDislocation;
        const double strength =
            blockBreak ? NO_DISLOCATION_STRENGTH_MULTIPLIER
                       : static_cast<double>(settings.breakStrengthMultiplier);

        if (force || baseline.strength != strength) {
            ApplyThresholds(willie, willie->Bones_Linear_Breakable_Limits, baseline.linear, strength, true, !blockBreak);
            ApplyThresholds(
                willie, willie->Bones_Angular_Breakable_Limits, baseline.angular, strength, false, !blockBreak
            );
            baseline.strength = strength;
        }
        ApplyDislocationConstraints(willie, baseline.constraints, blockBreak);

        const float mass = settings.massMultiplier;
        if (force || baseline.mass != mass) {
            ApplyMassScale(willie, mass);
            baseline.mass = mass;
        }

        if (blockBreak) {
            willie->Arm_R_Broken = willie->Arm_L_Broken = willie->Leg_R_Broken = willie->Leg_L_Broken =
                willie->Back_Broken = willie->Head_Broken = false;
            willie->Arm_R_Dislocated = willie->Arm_L_Dislocated = willie->Leg_R_Dislocated =
                willie->Leg_L_Dislocated = willie->Neck_Dislocated = willie->Spine_Dislocated = false;
        }
    }

    void ApplyToScope(const RuntimeContextSnapshot& runtime, bool playerScope, const Settings& settings, bool force) {
        if (playerScope) {
            Apply(runtime.player, settings, force);
            return;
        }

        ForEachEnemy(runtime, [&](SDK::AWillie_BP_C* willie) {
            Apply(willie, settings, force);
        });
    }

    void BreakAll(SDK::AWillie_BP_C* willie) {
        willie->Break_Arm_L();
        willie->Break_Arm_R();
        willie->Break_Leg_L();
        willie->Break_Leg_R();
        willie->Break_Back();
        willie->Break_Head();
    }

    void BreakEnemies(const RuntimeContextSnapshot& runtime) {
        ForEachEnemy(runtime, BreakAll);
    }
}
