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

RuntimeContextSnapshot ModContext::RefreshGameThreadCache() {
    RuntimeContextSnapshot next{};

    next.world = SDK::UWorld::GetWorld();
    if (next.world) {
        next.worldSettings = next.world->K2_GetWorldSettings();
        auto* gi = next.world->OwningGameInstance;
        if (gi && gi->LocalPlayers.Num()) {
            auto* lp = gi->LocalPlayers[0];
            if (lp) next.controller = lp->PlayerController;
        }
    }
    if (next.controller) {
        auto* pawn = next.controller->Pawn;
        next.player =
            (pawn && pawn->IsA(SDK::AWillie_BP_C::StaticClass())) ? static_cast<SDK::AWillie_BP_C*>(pawn) : nullptr;
    }

    gameThreadSnapshot = next;
    renderWorld.store(next.world, std::memory_order_release);
    renderPlayer.store(next.player, std::memory_order_release);
    renderController.store(next.controller, std::memory_order_release);
    renderWorldSettings.store(next.worldSettings, std::memory_order_release);
    return next;
}

RuntimeContextSnapshot ModContext::GetRenderSnapshot() const noexcept {
    return {
        renderWorld.load(std::memory_order_acquire),
        renderPlayer.load(std::memory_order_acquire),
        renderController.load(std::memory_order_acquire),
        renderWorldSettings.load(std::memory_order_acquire),
    };
}
