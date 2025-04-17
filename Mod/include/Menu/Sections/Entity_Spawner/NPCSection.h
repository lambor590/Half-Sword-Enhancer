#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"

struct NPCTypeInfo {
    const char* displayName;
    const char* className;
};

class NPCSection : public CollapsibleSection {
private:
    static inline int spawnEnemyKey = 0x4E; // N
    static inline float spawnDistanceForward = 200.0f;
    static inline float spawnDistanceUp = 0.0f;
    static inline float spawnScale = 1.0f;
    static inline int npcTeam = 0;
    static inline int npcTypeIndex = 0;

    static inline const NPCTypeInfo npcTypes[] = {
        { "Regular", "Willie_BP_C" },
        { "No Brain", "Willie_BP_NoBrain_C" },
        { "Boss 1", "Willie_BP_Boss_1_C" },
        { "Boss 2", "Willie_BP_Boss_2_C" },
        { "Boss 3", "Willie_BP_Boss_3_C" },
        { "Boss 4", "Willie_BP_Boss_4_C" },
        { "Boss 5", "Willie_BP_Boss_5_C" },
        { "Boss 6", "Willie_BP_Boss_6_C" },
        { "Boss 7", "Willie_BP_Boss_7_C" },
        { "Boss 8", "Willie_BP_Boss_8_C" },
        { "Boss 9 (Baron)", "Willie_BP_Boss_9_BARON_C" }
    };
    static inline const int npcTypesCount = sizeof(npcTypes) / sizeof(npcTypes[0]);

    static inline const char* npcTypeNames[sizeof(npcTypes) / sizeof(npcTypes[0])];

    static void initNPCTypeNames() {
        static bool initialized = false;
        if (!initialized) {
            for (int i = 0; i < npcTypesCount; i++) {
                npcTypeNames[i] = npcTypes[i].displayName;
            }
            initialized = true;
        }
    }

    const char* getNPCClassName() const {
        return (npcTypeIndex >= 0 && npcTypeIndex < npcTypesCount)
            ? npcTypes[npcTypeIndex].className
            : npcTypes[0].className;
    }

public:
    NPCSection() : CollapsibleSection("NPC") {
        initNPCTypeNames();
        std::initializer_list<Parameter> spawnEnemyParams = {
            Parameter("distance_forward", "Distance Forward", &spawnDistanceForward, 100.0f, 500.0f),
            Parameter("distance_up", "Distance Up", &spawnDistanceUp, 0.0f, 300.0f),
            Parameter("scale", "Scale", &spawnScale, 0.1f, 4.0f),
            Parameter("team", "Team", &npcTeam, 0, 9)
        };

        BindWithParams("Spawn NPC", &spawnEnemyKey, spawnEnemyParams, [this]() {
            SDK::FTransform spawnTransform = player->GetTransform();
            spawnTransform.Translation += player->GetActorForwardVector() * spawnDistanceForward;
            spawnTransform.Translation.Z += spawnDistanceUp;
            spawnTransform.Scale3D = SDK::FVector(spawnScale, spawnScale, spawnScale);
            SDK::AWillie_BP_C* npc = static_cast<SDK::AWillie_BP_C*>(Spawner::SpawnActor(world, getNPCClassName(), spawnTransform));
            npc->Team_Int = npcTeam;
        }, player, world);
    }

    void Render() override {
        if (ImGui::CollapsingHeader(name.c_str())) {
            for (auto& function : functions) {
                function->Render();
            }

            if (player)
                ImGui::Text("Your team is %d. Spawn an NPC to refresh it.", player->Team_Int);
            else
                ImGui::Text("Spawn an NPC to see your team.");

            ImGui::Text("NPC Type");
            if (ImGui::Combo("##NPCTypeSelector", &npcTypeIndex, npcTypeNames, npcTypesCount)) {
                // Selection changed
            }
        }
    }
};