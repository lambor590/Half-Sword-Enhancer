#pragma once

#include "SDK/Engine_classes.hpp"

class GameInstances {
protected:
    static GameInstances* s_instance;
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

    SDK::UWorld* GetWorld() const { return m_world; }
    SDK::APlayerController* GetPlayerController() const { return m_playerController; }
    SDK::APawn* GetPlayerPawn() const { return m_playerPawn; }

    virtual bool Update() {
        m_world = SDK::UWorld::GetWorld();
        m_playerController = m_world->OwningGameInstance->LocalPlayers[0]->PlayerController;
        m_playerPawn = m_playerController->K2_GetPawn();
    }

    GameInstances(const GameInstances&) = delete;
    GameInstances& operator=(const GameInstances&) = delete;
};