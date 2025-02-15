#pragma once

#include "SDK/Engine_classes.hpp"

class GameInstances {
protected:
    inline static GameInstances* s_instance = nullptr;
    SDK::UWorld* m_world = nullptr;
    SDK::APlayerController* m_playerController = nullptr;
    SDK::APawn* m_playerPawn = nullptr;

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
    SDK::APawn*& GetPlayerPawn() { return m_playerPawn; }

    virtual void Update() {
        m_world = SDK::UWorld::GetWorld();
        if (m_world) {
            m_playerController = m_world->OwningGameInstance->LocalPlayers[0]->PlayerController;
            if (m_playerController) {
                m_playerPawn = m_playerController->K2_GetPawn();
            }
        }
    }

    GameInstances(const GameInstances&) = delete;
    GameInstances& operator=(const GameInstances&) = delete;
};