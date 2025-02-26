#pragma once

#include "SDK/Engine_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

class ComponentValidator {
private:
    inline static SDK::UWorld* s_world = nullptr;

public:
    template<typename T>
    static bool Validate(T*& component) {
        return false;
    }
};

template<>
bool ComponentValidator::Validate<SDK::UWorld>(SDK::UWorld*& world);

template<>
bool ComponentValidator::Validate<SDK::AWorldSettings>(SDK::AWorldSettings*& worldSettings);

template<>
bool ComponentValidator::Validate<SDK::APlayerController>(SDK::APlayerController*& playerController);

template<>
bool ComponentValidator::Validate<SDK::AWillie_BP_C>(SDK::AWillie_BP_C*& playerPawn); 