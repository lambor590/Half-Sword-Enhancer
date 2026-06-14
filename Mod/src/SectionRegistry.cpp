#include "Menu/SectionRegistry.h"

#include "Core/ModContext.h"
#include "Menu/MenuManager.h"
#include "Menu/Sections/Equipment/ArmorEditorSection.h"
#include "Menu/Sections/Equipment/LoadoutManagerSection.h"
#include "Menu/Sections/Equipment/WeaponEditorSection.h"
#include "Menu/Sections/Player/PlayerAbilitiesSection.h"
#include "Menu/Sections/Player/PlayerEditorSection.h"
#include "Menu/Sections/Settings/AssetOverridesSection.h"
#include "Menu/Sections/Settings/GraphicsSection.h"
#include "Menu/Sections/Settings/GuiSection.h"
#include "Menu/Sections/Spawner/ItemSpawnerSection.h"
#include "Menu/Sections/Spawner/NPCEditorSection.h"
#include "Menu/Sections/World/AIDirectorSection.h"
#include "Menu/Sections/World/MapLoaderSection.h"
#include "Menu/Sections/World/SkyEditorSection.h"
#include "Menu/Sections/World/WorldActionsSection.h"
#include "Menu/Sections/World/WorldEditorSection.h"

void RegisterModSections(MenuManager& menu, ModContext& ctx) {
    menu.AddSection<GuiSection>(ctx);
    menu.AddSection<AssetOverridesSection>(ctx);
    menu.AddSection<GraphicsSection>(ctx);
    menu.AddSection<AIDirectorSection>(ctx);
    menu.AddSection<WorldActionsSection>(ctx);
    menu.AddSection<WorldEditorSection>(ctx);
    menu.AddSection<SkyEditorSection>(ctx);
    menu.AddSection<MapLoaderSection>(ctx);
    menu.AddSection<PlayerAbilitiesSection>(ctx);
    menu.AddSection<PlayerEditorSection>(ctx);
    menu.AddSection<NPCEditorSection>(ctx);
    menu.AddSection<ItemSpawnerSection>(ctx);
    menu.AddSection<ArmorEditorSection>(ctx);
    menu.AddSection<WeaponEditorSection>(ctx);
    menu.AddSection<LoadoutManagerSection>(ctx);
}
