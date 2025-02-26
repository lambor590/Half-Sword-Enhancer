#include "ComponentValidator.h"

template<>
bool ComponentValidator::Validate<SDK::UWorld>(SDK::UWorld*& world) {
    if (!s_world) s_world = SDK::UWorld::GetWorld();
    return (world = s_world) != nullptr;
}

template<>
bool ComponentValidator::Validate<SDK::AWorldSettings>(SDK::AWorldSettings*& worldSettings) {
    SDK::UWorld* world;
    return Validate(world) && (worldSettings = world->K2_GetWorldSettings());
}

template<>
bool ComponentValidator::Validate<SDK::APlayerController>(SDK::APlayerController*& playerController) {
    SDK::UWorld* world;
    return Validate(world) && 
           (playerController = world->OwningGameInstance->LocalPlayers[0]->PlayerController);
}

template<>
bool ComponentValidator::Validate<SDK::AWillie_BP_C>(SDK::AWillie_BP_C*& playerPawn) {
    SDK::APlayerController* controller;
    return Validate(controller) && 
           (playerPawn = static_cast<SDK::AWillie_BP_C*>(controller->Pawn)) &&
           playerPawn->Class->Name == SDK::AWillie_BP_C::StaticClass()->Name;
}