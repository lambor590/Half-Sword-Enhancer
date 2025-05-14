#pragma once

#include "ComponentValidator.h"

class GameInstances {
private:
    GameInstances() = default;

public:
    static GameInstances& Get() {
        static GameInstances instance;
        return instance;
    }

    template<typename T>
    bool ValidateComponent(T*& component) {
        return ComponentValidator::Validate(component);
    }

    GameInstances(const GameInstances&) = delete;
    GameInstances& operator=(const GameInstances&) = delete;
};