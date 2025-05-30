#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "Menu/ICollapsibleSection.h"
#include "Menu/Utils/Spawner.h"
#include "DefaultStyle.h"

#define WILLIE_PATH(s) "/Game/Character/Blueprints" s
#define BOSSES_PATH(s) WILLIE_PATH("/Unique/Bosses") s

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
    static inline bool bodyguard = false;
    static inline int npcTeam = 0;
    static inline int npcTypeIndex = 0;

    static inline const NPCTypeInfo npcTypes[] = {
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
            Parameter("bodyguard", "Bodyguard", &bodyguard),
            Parameter("distance_forward", "Distance Forward", &spawnDistanceForward, 100.0f, 500.0f),
            Parameter("distance_up", "Distance Up", &spawnDistanceUp, 0.0f, 300.0f),
            Parameter("scale", "Scale", &spawnScale, 0.1f, 4.0f),
            Parameter("team", "Team", &npcTeam, 0, 9)
        };

        Function("Spawn NPC")
            .WithKey(&spawnEnemyKey)
            .WithParams(spawnEnemyParams)
            .Action([this]() {
                SDK::FTransform spawnTransform = player->GetTransform();
                spawnTransform.Translation += player->GetActorForwardVector() * spawnDistanceForward;
                spawnTransform.Translation.Z += spawnDistanceUp;
                spawnTransform.Scale3D = SDK::FVector(spawnScale, spawnScale, spawnScale);
                
                Spawner::SpawnActor(world, getNPCClassName(), spawnTransform, [this](SDK::AActor* actor) {
                    SDK::AWillie_BP_C* npc = static_cast<SDK::AWillie_BP_C*>(actor);
                    if (npc) {
                        if (bodyguard) {
                            player->Team_Int = 1337;
                            npc->Team_Int = player->Team_Int;
                        } else {
                            npc->Team_Int = npcTeam;
                        }
                    }
                });
            }, player, world);
    }

    void Render() override {
        bool isOpen = ImGui::CollapsingHeader(name.c_str());
        
        if (isOpen) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 25.0f);
            
            ImGui::Indent(10.0f);
            ImGui::Spacing();

            for (auto& function : functions) {
                function->Render();
                ImGui::Spacing();
            }

            ImGui::Text("NPC Type");
            if (ImGui::Combo("##NPCTypeSelector", &npcTypeIndex, npcTypeNames, npcTypesCount)) {
                // Selection changed
            }
            
            ImGui::Unindent(10.0f);
            ImGui::PopStyleVar(3);
        }
    }
};