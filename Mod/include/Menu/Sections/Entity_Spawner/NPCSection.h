#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"

class NPCSection : public CollapsibleSection {
private:
    static inline int spawnEnemyKey = 0x4E; // N
    static inline float spawnDistanceForward = 200.0f;
    static inline float spawnDistanceUp = 0.0f;
    static inline float spawnScale = 1.0f;
    static inline bool spawnNPCAggressive = true;
    static inline int npcTeam = 0;

public:
    NPCSection() : CollapsibleSection("NPC") {
        std::initializer_list<Parameter> spawnEnemyParams = {
            Parameter("aggresive", "Is Aggressive", &spawnNPCAggressive),
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
            SDK::AWillie_BP_C* npc = static_cast<SDK::AWillie_BP_C*>(Spawner::SpawnActor(world, spawnNPCAggressive ? "Willie_BP_C" : "Willie_BP_NoBrain_C", spawnTransform));
            npc->Team_Int = npcTeam;
        }, player, world);
    }
};