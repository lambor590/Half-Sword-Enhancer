#include "Core/ModContext.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"
#include "SDK/Engine_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

ModContext& ModContext::Get() {
    static ModContext instance;
    return instance;
}

ModContext::ModContext() : gameHook(GameHook::Get()), configManager(ConfigManager::Get()) {}

void ModContext::RefreshCache() {
    world = SDK::UWorld::GetWorld();
    worldSettings = nullptr;
    controller = nullptr;
    player = nullptr;

    if (world) {
        worldSettings = world->K2_GetWorldSettings();
        auto* gi = world->OwningGameInstance;
        if (gi && gi->LocalPlayers.Num()) {
            auto* lp = gi->LocalPlayers[0];
            if (lp) controller = lp->PlayerController;
        }
    }
    if (controller) {
        auto* pawn = controller->Pawn;
        player =
            (pawn && pawn->IsA(SDK::AWillie_BP_C::StaticClass())) ? static_cast<SDK::AWillie_BP_C*>(pawn) : nullptr;
    }
}
