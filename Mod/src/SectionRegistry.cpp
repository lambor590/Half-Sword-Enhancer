#include "Menu/SectionRegistry.h"
#include "Menu/MenuManager.h"
#include "Menu/Section.h"

SectionRegistry& SectionRegistry::Get() {
    static SectionRegistry instance;
    return instance;
}

void SectionRegistry::Add(MenuTab tab, Factory factory) {
    entries.push_back({tab, std::move(factory)});
}

void SectionRegistry::CreateAll(MenuManager& menu, ModContext& ctx) const {
    for (const auto& entry : entries) {
        menu.AddSection(entry.tab, entry.factory(ctx));
    }
}
