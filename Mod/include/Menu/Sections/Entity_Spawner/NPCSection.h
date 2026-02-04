#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/SectionConfig.h"
#include "Utils/Spawner.h"
#include "DefaultStyle.h"

#define WILLIE_PATH(s) "/Game/Character/Blueprints" s
#define BOSSES_PATH(s) WILLIE_PATH("/Unique/Bosses") s

struct NPCTypeInfo {
    const char* displayName;
    const char* className;
};

class NPCSection : public CollapsibleSection {
private:
    static constexpr int SPECIAL_TEAM_ID = 1337;

    SectionConfig::NPCConfig& cfg = SectionConfig::npc;

    static constexpr NPCTypeInfo npcTypes[] = {
        { "Regular", WILLIE_PATH("/Willie_BP.Willie_BP_C") },
        { "No Brain", WILLIE_PATH("/Willie_BP_NoBrain.Willie_BP_NoBrain_C") },
        { "Boss 1", BOSSES_PATH("/Willie_BP_Boss_1.Willie_BP_Boss_1_C") },
        { "Boss 2", BOSSES_PATH("/Willie_BP_Boss_2.Willie_BP_Boss_2_C") },
        { "Boss 3", BOSSES_PATH("/Willie_BP_Boss_3.Willie_BP_Boss_3_C") },
        { "Boss 4", BOSSES_PATH("/Willie_BP_Boss_4.Willie_BP_Boss_4_C") },
        { "Boss 5", BOSSES_PATH("/Willie_BP_Boss_5.Willie_BP_Boss_5_C") },
        { "Boss 6", BOSSES_PATH("/Willie_BP_Boss_6.Willie_BP_Boss_6_C") },
        { "Boss 7", BOSSES_PATH("/Willie_BP_Boss_7.Willie_BP_Boss_7_C") },
        { "Boss 8", BOSSES_PATH("/Willie_BP_Boss_8.Willie_BP_Boss_8_C") },
        { "Boss 9 (Baron)", BOSSES_PATH("/Willie_BP_Boss_9_BARON.Willie_BP_Boss_9_BARON_C") }
    };
    static constexpr int npcTypesCount = sizeof(npcTypes) / sizeof(npcTypes[0]);

    static constexpr const char* npcTypeNames[] = {
        "Regular", "No Brain", "Boss 1", "Boss 2", "Boss 3",
        "Boss 4", "Boss 5", "Boss 6", "Boss 7", "Boss 8", "Boss 9 (Baron)"
    };

    const char* getNPCClassName() const noexcept {
        if (cfg.npcTypeIndex >= 0 && cfg.npcTypeIndex < npcTypesCount) [[likely]] {
            return npcTypes[cfg.npcTypeIndex].className;
        }
        return npcTypes[0].className;
    }

public:
    NPCSection() : CollapsibleSection("NPC") {
        Function("Spawn NPC")
            .WithKey(&cfg.spawnEnemyKey)
            .WithParams({
                Parameter("bodyguard", "Bodyguard", &cfg.bodyguard, "Will join your team"),
                Parameter("snap_to_ground", "Snap to Ground", &cfg.snapToGround, "Automatically adjust height to touch the ground"),
                Parameter("distance_forward", "Distance Forward", &cfg.spawnDistanceForward, 100.0f, 500.0f, "How far in front the NPC appears"),
                Parameter("distance_up", "Distance Up", &cfg.spawnDistanceUp, 0.0f, 300.0f, "Height offset for spawn position"),
                Parameter("scale", "Scale", &cfg.spawnScale, 0.1f, 4.0f, "Size multiplier for the spawned NPC. Adjust the height offset to match the scale so the game doesn't crash."),
                Parameter("team", "Team", &cfg.npcTeam, 0, 9, "Assign the NPC to a team number. 0-4 are the default teams. 0 means no team.")
            })
            .WithTooltip("Spawns an NPC with configurable type, position and team settings")
            .Action([this]() {
                SDK::FTransform spawnTransform = player->GetTransform();
                spawnTransform.Translation += player->GetActorForwardVector() * cfg.spawnDistanceForward;
                spawnTransform.Translation.Z += cfg.spawnDistanceUp;
                spawnTransform.Scale3D = SDK::FVector(cfg.spawnScale, cfg.spawnScale, cfg.spawnScale);

                Spawner::SpawnActor(world, getNPCClassName(), spawnTransform, [this](SDK::AActor* actor) {
                    auto* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (npc) [[likely]] {
                        if (cfg.bodyguard) [[unlikely]] {
                            player->Team_Int = SPECIAL_TEAM_ID;
                            npc->Team_Int = player->Team_Int;
                        } else {
                            npc->Team_Int = cfg.npcTeam;
                        }
                    }
                }, cfg.snapToGround);
            }, player, world);
    }

    void RenderContent() override {
        const SectionStyle::StyleRAII style;

        for (auto& function : functions) {
            function->Render();
            ImGui::Spacing();
        }

        ImGui::Text("NPC Type");
        TooltipHelper::ShowTooltip("Choose which NPC class to spawn");

        ImGui::Combo("##NPCTypeSelector", &cfg.npcTypeIndex, npcTypeNames, npcTypesCount);
    }
};
