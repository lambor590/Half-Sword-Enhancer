#include "Utils/BoneControl.h"

#include <algorithm>
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

        std::vector<std::pair<SDK::AWillie_BP_C*, WillieBaseline>> g_baselines;

        WillieBaseline& BaselineFor(SDK::AWillie_BP_C* willie) {
            for (auto& [storedWillie, baseline] : g_baselines) {
                if (storedWillie == willie) return baseline;
            }
            return g_baselines.emplace_back(willie, WillieBaseline{}).second;
        }

        bool IsPlayer(SDK::AWillie_BP_C* willie) noexcept {
            if (!willie) return false;

            const auto& snapshot = ModContext::Get().GetRenderSnapshot();
            return willie == snapshot.player || willie->Player;
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

        bool IsDislocationConstraint(SDK::AWillie_BP_C* willie, SDK::UPhysicsConstraintComponent* component) {
            bool matched = false;
            ForEachDislocationConstraint(willie, [&](SDK::UPhysicsConstraintComponent* candidate) {
                matched |= candidate == component;
            });
            return matched;
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

        void ApplyConstraintBreakableToMesh(
            SDK::USkeletalMeshComponent* mesh, const SDK::FName& bone, bool linear, double threshold, bool breakable
        ) {
            if (!mesh || bone.IsNone()) return;

            SDK::TArray<SDK::FConstraintInstanceAccessor> constraints;
            mesh->GetConstraintsFromBody(bone, true, true, false, &constraints);
            const auto clampedThreshold =
                static_cast<float>(std::clamp(threshold, 0.0, static_cast<double>((std::numeric_limits<float>::max)())));

            for (auto& constraint : constraints) {
                if (linear) {
                    SDK::UConstraintInstanceBlueprintLibrary::SetLinearBreakable(
                        constraint, breakable, clampedThreshold
                    );
                } else {
                    SDK::UConstraintInstanceBlueprintLibrary::SetAngularBreakable(
                        constraint, breakable, clampedThreshold
                    );
                }
            }
        }

        void ApplyConstraintBreakable(
            SDK::AWillie_BP_C* willie, const SDK::FName& bone, bool linear, double threshold, bool breakable
        ) {
            ForEachBoneMesh(willie, [&](SDK::USkeletalMeshComponent* mesh) {
                ApplyConstraintBreakableToMesh(mesh, bone, linear, threshold, breakable);
            });
        }

        void ApplyThresholds(
            SDK::AWillie_BP_C* willie, SDK::TMap<SDK::FName, double>& map, const ThresholdBaseline& baseline,
            double multiplier, bool linear, bool breakable
        ) {
            for (auto& entry : map) {
                const double scaled = BaselineValueFor(baseline, entry.Key(), entry.Value()) * multiplier;
                entry.Value() = scaled;
                ApplyConstraintBreakable(willie, entry.Key(), linear, scaled, breakable);
            }
        }

        void ApplyMassScaleToBones(
            SDK::USkeletalMeshComponent* mesh, const SDK::TArray<SDK::FName>& bones, float multiplier
        ) {
            for (const auto& bone : bones) {
                if (!bone.IsNone()) mesh->SetMassScale(bone, multiplier);
            }
        }

        void ApplyMassScaleToThresholdBones(
            SDK::USkeletalMeshComponent* mesh, SDK::TMap<SDK::FName, double>& map, float multiplier
        ) {
            for (auto& entry : map) {
                if (!entry.Key().IsNone()) mesh->SetMassScale(entry.Key(), multiplier);
            }
        }

        void ApplyMassScaleToMesh(SDK::AWillie_BP_C* willie, SDK::USkeletalMeshComponent* mesh, float multiplier) {
            mesh->SetAllMassScale(multiplier);

            for (const auto* bones :
                 {&willie->Rigid_Bones, &willie->Upper_Torso_Bones, &willie->Lower_Torso_Bones, &willie->R_Arm_Bones,
                  &willie->L_Arm_Bones, &willie->R_Leg_Bones, &willie->L_Leg_Bones, &willie->Neck_Bones,
                  &willie->Head_Bones, &willie->Breakable_Bones_List}) {
                ApplyMassScaleToBones(mesh, *bones, multiplier);
            }
            ApplyMassScaleToThresholdBones(mesh, willie->Bones_Linear_Breakable_Limits, multiplier);
            ApplyMassScaleToThresholdBones(mesh, willie->Bones_Angular_Breakable_Limits, multiplier);
        }

        void ApplyMassScale(SDK::AWillie_BP_C* willie, float multiplier) {
            ForEachBoneMesh(willie, [&](SDK::USkeletalMeshComponent* mesh) {
                ApplyMassScaleToMesh(willie, mesh, multiplier);
            });
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
        if (playerScope) return IsPlayer(willie);
        return willie && !IsPlayer(willie) && willie != PossessState::GetOriginalPawn();
    }

    bool ShouldCancelBreak(GameHook::ProcessEventContext& context, SDK::AWillie_BP_C* willie) {
        if (context.object->IsA(SDK::UPhysicsConstraintComponent::StaticClass())) {
            return IsDislocationConstraint(willie, static_cast<SDK::UPhysicsConstraintComponent*>(context.object));
        }
        return true;
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
