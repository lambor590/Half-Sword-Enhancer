#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "MenuTab.h"

class Section;
class ModContext;
class MenuManager;

/// Each section .cpp adds itself with the REGISTER_SECTION macro at file scope.
class SectionRegistry {
public:
    using Factory = std::function<std::unique_ptr<Section>(ModContext&)>;

    struct Entry {
        MenuTab tab;
        Factory factory;
    };

    static SectionRegistry& Get();

    /// Called during static initialization.
    void Add(MenuTab tab, Factory factory);

    void CreateAll(MenuManager& menu, ModContext& ctx) const;

    SectionRegistry(const SectionRegistry&) = delete;
    SectionRegistry& operator=(const SectionRegistry&) = delete;

private:
    SectionRegistry() = default;
    std::vector<Entry> entries;
};

struct SectionRegistrar {
    SectionRegistrar(MenuTab tab, SectionRegistry::Factory factory) {
        SectionRegistry::Get().Add(tab, std::move(factory));
    }
};

/// Place this macro in a section's .cpp file to auto-register it.
/// Example: REGISTER_SECTION(PlayerAbilitiesSection, MenuTab::Player)
#define REGISTER_SECTION(Type, Tab)                                                                 \
    static SectionRegistrar s_register##Type(Tab, [](ModContext& ctx) -> std::unique_ptr<Section> { \
        return std::make_unique<Type>(ctx);                                                         \
    })
