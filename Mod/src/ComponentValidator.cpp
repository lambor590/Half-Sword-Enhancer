#include "ComponentValidator.h"

template <> bool ComponentValidator::Validate<SDK::UWorld>(SDK::UWorld*& world) {
    return (world = SDK::UWorld::GetWorld()) != nullptr;
}

template <> bool ComponentValidator::Validate<SDK::AWorldSettings>(SDK::AWorldSettings*& worldSettings) {
    SDK::UWorld* world;
    return Validate(world) && (worldSettings = world->K2_GetWorldSettings());
}

template <> bool ComponentValidator::Validate<SDK::APlayerController>(SDK::APlayerController*& playerController) {
    SDK::UWorld* world;
    if (!Validate(world)) return false;
    auto* gi = world->OwningGameInstance;
    if (!gi || !gi->LocalPlayers.Num()) return false;
    auto* lp = gi->LocalPlayers[0];
    return lp && (playerController = lp->PlayerController);
}

template <> bool ComponentValidator::Validate<SDK::AWillie_BP_C>(SDK::AWillie_BP_C*& playerPawn) {
    SDK::APlayerController* controller;
    return Validate(controller) && (playerPawn = static_cast<SDK::AWillie_BP_C*>(controller->Pawn)) &&
           playerPawn->IsA(SDK::AWillie_BP_C::StaticClass());
}
