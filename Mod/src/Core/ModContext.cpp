#include "Core/ModContext.h"
#include "Utils/GameClass.h"
#include "SDK/Engine_classes.hpp"

namespace {
    template <typename T>
    void PublishRenderObject(T* object, std::atomic<std::int32_t>& index, std::atomic<T*>& pointer) noexcept {
        index.store(object ? object->Index : -1, std::memory_order_relaxed);
        pointer.store(object, std::memory_order_release);
    }

    template <typename T>
    T* LoadLiveRenderObject(const std::atomic<T*>& pointer, const std::atomic<std::int32_t>& index) noexcept {
        auto* object = pointer.load(std::memory_order_acquire);
        if (!object) return nullptr;

        const auto cachedIndex = index.load(std::memory_order_relaxed);
        if (cachedIndex < 0 || SDK::UObject::GObjects->GetByIndex(cachedIndex) != object) return nullptr;

        const auto* uobject = static_cast<const SDK::UObject*>(object);
        if (!uobject->Class || (uobject->Flags & SDK::EObjectFlags::BeginDestroyed) ||
            (uobject->Flags & SDK::EObjectFlags::FinishDestroyed))
            return nullptr;
        return object;
    }
}

ModContext& ModContext::Get() {
    static ModContext instance;
    return instance;
}

RuntimeContextSnapshot ModContext::RefreshGameThreadCache() {
    RuntimeContextSnapshot next{};

    next.world = SDK::UWorld::GetWorld();
    if (next.world) {
        next.worldSettings = next.world->K2_GetWorldSettings();
        auto* gi = next.world->OwningGameInstance;
        auto* localPlayer = gi && gi->LocalPlayers.Num() > 0 ? gi->LocalPlayers[0] : nullptr;
        next.controller = localPlayer ? localPlayer->PlayerController : nullptr;
    }
    if (next.controller) {
        auto* pawn = next.controller->Pawn;
        next.player = GameClass::IsWillie(pawn) ? static_cast<SDK::AWillie_BP_C*>(pawn) : nullptr;
    }

    PublishRenderObject(next.world, renderWorldIndex, renderWorld);
    PublishRenderObject(next.player, renderPlayerIndex, renderPlayer);
    PublishRenderObject(next.controller, renderControllerIndex, renderController);
    PublishRenderObject(next.worldSettings, renderWorldSettingsIndex, renderWorldSettings);
    return next;
}

RuntimeContextSnapshot ModContext::GetRenderSnapshot() const noexcept {
    auto* world = LoadLiveRenderObject(renderWorld, renderWorldIndex);
    if (!world) return {};

    auto* controller = LoadLiveRenderObject(renderController, renderControllerIndex);
    auto* player = controller ? LoadLiveRenderObject(renderPlayer, renderPlayerIndex) : nullptr;
    auto* worldSettings = LoadLiveRenderObject(renderWorldSettings, renderWorldSettingsIndex);
    return {world, player, controller, worldSettings};
}
