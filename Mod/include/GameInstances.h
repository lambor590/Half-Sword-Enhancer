#pragma once

#include "SDK/Engine_classes.hpp"
#include "SDK/Willie_BP_classes.hpp"

class GameInstances {
protected:
    inline static GameInstances* s_instance = nullptr;
    SDK::UWorld* m_world = nullptr;
    SDK::APlayerController* m_playerController = nullptr;
    SDK::AWillie_BP_C* m_playerPawn = nullptr;
    SDK::AWorldSettings* m_worldSettings = nullptr;

    GameInstances() = default;

public:
    static GameInstances& Get() {
        if (!s_instance) {
            s_instance = new GameInstances();
        }
        return *s_instance;
    }

    SDK::UWorld*& GetWorld() { return m_world; }
    SDK::APlayerController*& GetPlayerController() { return m_playerController; }
    SDK::AWillie_BP_C*& GetPlayerPawn() { return m_playerPawn; }
    SDK::AWorldSettings*& GetWorldSettings() { return m_worldSettings; }

    virtual void Update() {
        m_world = SDK::UWorld::GetWorld();
        if (m_world) {
            m_worldSettings = m_world->K2_GetWorldSettings();
            m_playerController = m_world->OwningGameInstance->LocalPlayers[0]->PlayerController;
            if (m_playerController) {
                m_playerPawn = static_cast<SDK::AWillie_BP_C*>(m_playerController->AcknowledgedPawn);
            }
        }
    }

    GameInstances(const GameInstances&) = delete;
    GameInstances& operator=(const GameInstances&) = delete;
};