#include "Core/ModContext.h"
#include "ComponentValidator.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"

ModContext& ModContext::Get() {
    static ModContext instance;
    return instance;
}

ModContext::ModContext() : gameHook(GameHook::Get()), configManager(ConfigManager::Get()) {}

void ModContext::RefreshCache() {
    ComponentValidator::Validate(world);
    ComponentValidator::Validate(controller);
    ComponentValidator::Validate(player);
    ComponentValidator::Validate(worldSettings);
}
