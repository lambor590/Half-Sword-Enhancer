#include "GlobalDefinitions.h"
#include "Hooks/GameHook.h"
#include "ConfigManager.h"

GameHook* g_GameHook = &GameHook::Get();
ConfigManager& g_ConfigManager = ConfigManager::Get();