#include "Utils/BoneControl.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "SDK/Engine_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"
#include "Utils/ActorUtils.h"
#include "Utils/EngineFrame.h"
#include "Utils/PossessState.h"

namespace BoneControl {
    namespace {
        constexpr float MASS_SCALE_EPSILON = 0.001f;
        // Bone Strength's slider reaches 0; a bone this much weaker than the game's already breaks at a touch.
        constexpr double WEAKEST_STRENGTH = 0.05;
        constexpr std::size_t DISLOCATION_PARTS = 6;
        // Bone Control runs on every tick of a character it covers; a longer gap means it was off meanwhile.
        constexpr std::int64_t MOST_FRAMES_BETWEEN_TICKS = 2;

        struct ConstraintBaseline {
            SDK::UPhysicsConstraintComponent* component = nullptr;
            bool linearBreakable = false;
            float linearThreshold = 0.0f;
            bool angularBreakable = false;
            float angularThreshold = 0.0f;
        };

        // A character's own Bone Snapping, which the game copies from its settings and scales the damage that breaks
        // a bone by, the value the mod last gave it, and how dislocated each part was when last seen.
        struct WillieBaseline {
            int objectIndex = -1;
            std::vector<ConstraintBaseline> constraints;
            double boneSnapping = -1.0;
            double appliedSnapping = -1.0;
            std::array<double, DISLOCATION_PARTS> dislocation{};
            std::int64_t seenFrame = -1;
            bool blockedDislocation = false;
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

        void SetBreakable(
            SDK::UPhysicsConstraintComponent* component, bool linearBreakable, float linearThreshold,
            bool angularBreakable, float angularThreshold
        ) {
            // This runs on every Willie tick: skip the engine calls (and the physics update) when the constraint
            // already holds these values.
            const auto& profile = component->ConstraintInstance.ProfileInstance;
            if (profile.bLinearBreakable != linearBreakable || profile.LinearBreakThreshold != linearThreshold)
                component->SetLinearBreakable(linearBreakable, linearThreshold);
            if (profile.bAngularBreakable != angularBreakable || profile.AngularBreakThreshold != angularThreshold)
                component->SetAngularBreakable(angularBreakable, angularThreshold);
        }

        void ApplyDislocationConstraints(
            SDK::AWillie_BP_C* willie, std::vector<ConstraintBaseline>& baseline, bool blockBreak
        ) {
            ForEachDislocationConstraint(willie, [&](SDK::UPhysicsConstraintComponent* component) {
                const auto& original = CaptureConstraint(component, baseline);
                constexpr float UNBREAKABLE = (std::numeric_limits<float>::max)();
                if (blockBreak) {
                    SetBreakable(component, false, UNBREAKABLE, false, UNBREAKABLE);
                } else {
                    SetBreakable(
                        component, original.linearBreakable, original.linearThreshold, original.angularBreakable,
                        original.angularThreshold
                    );
                }
            });
        }

        // The game breaks a bone when a blow's damage, scaled by the character's Bone Snapping, passes the bone's
        // limit. The game sets Bone Snapping from its settings, so a value the mod did not give is the game's own.
        void ApplyBoneSnapping(SDK::AWillie_BP_C* willie, WillieBaseline& baseline, double scale) {
            if (willie->Bone_Snapping != baseline.appliedSnapping) baseline.boneSnapping = willie->Bone_Snapping;
            willie->Bone_Snapping = baseline.boneSnapping * scale;
            baseline.appliedSnapping = willie->Bone_Snapping;
        }

        // Strained joints dislocate as their dislocation state fills; a stronger bone fills it slower. Only what filled
        // since the last tick is scaled, so a character first seen, or seen again after a pause, keeps its state.
        void SlowDislocation(SDK::AWillie_BP_C* willie, WillieBaseline& baseline, double scale) {
            const auto frame = EngineFrame::Current();
            if (baseline.seenFrame < 0 || frame - baseline.seenFrame > MOST_FRAMES_BETWEEN_TICKS) scale = 1.0;
            baseline.seenFrame = frame;
            const std::array<double*, DISLOCATION_PARTS> states{
                &willie->Neck_Dislocation_State, &willie->Arm_R_Dislocation_State, &willie->Arm_L_Dislocation_State,
                &willie->Back_Dislocation_State, &willie->Leg_R_Dislocation_State, &willie->Leg_L_Dislocation_State
            };
            for (std::size_t part = 0; part < DISLOCATION_PARTS; ++part) {
                auto& state = *states[part];
                const double previous = baseline.dislocation[part];
                if (state > previous) state = previous + (state - previous) * scale;
                baseline.dislocation[part] = state;
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
        const bool blockBreak = settings.blockDislocation;
        const double strength = (std::max)(static_cast<double>(settings.breakStrengthMultiplier), WEAKEST_STRENGTH);
        ApplyBoneSnapping(willie, baseline, blockBreak ? 0.0 : 1.0 / strength);
        SlowDislocation(willie, baseline, blockBreak ? 0.0 : 1.0 / strength);
        if (blockBreak || baseline.blockedDislocation) willie->Dislocation_Blocked = blockBreak;
        baseline.blockedDislocation = blockBreak;
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
