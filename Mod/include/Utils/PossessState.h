#pragma once

#include "SDK/Willie_BP_classes.hpp"
#include "SDK/AIModule_classes.hpp"

namespace PossessState {
    inline SDK::AAIController* prevController = nullptr;
    inline SDK::APawn* originalPawn = nullptr;
    inline SDK::AWillie_BP_C* possessed = nullptr;
    inline SDK::UWorld* lastWorld = nullptr;

    inline void Reset() {
        prevController = nullptr;
        originalPawn = nullptr;
        possessed = nullptr;
    }

    inline SDK::AWillie_BP_C* GetOriginalPawn() {
        return originalPawn ? static_cast<SDK::AWillie_BP_C*>(originalPawn) : nullptr;
    }
}
