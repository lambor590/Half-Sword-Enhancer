#pragma once

#include "ComponentValidator.h"

class GameInstances {
protected:
    inline static GameInstances* s_instance = nullptr;
    GameInstances() = default;

public:
    static GameInstances& Get() {
        if (!s_instance) s_instance = new GameInstances();
        return *s_instance;
    }

    template<typename T>
    bool ValidateComponent(T*& component) {
        return ComponentValidator::Validate(component);
    }

    GameInstances(const GameInstances&) = delete;
    GameInstances& operator=(const GameInstances&) = delete;
};