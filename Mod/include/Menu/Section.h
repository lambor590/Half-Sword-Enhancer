#pragma once

#include "Core/ModContext.h"
#include "Menu/MenuTab.h"

class KeybindList;

struct SectionDefinition {
    MenuTab tab;
    const char* name;
    const char* description;
};

class Section {
protected:
    ModContext& ctx;
    const SectionDefinition* sectionDefinition;

    struct PlayerWorld {
        SDK::UWorld* world = nullptr;
        SDK::AWillie_BP_C* player = nullptr;
    };

    RuntimeContextSnapshot RenderSnapshot() const noexcept { return ctx.GetRenderSnapshot(); }

    SDK::AWillie_BP_C* RenderPlayer() const noexcept { return RenderSnapshot().player; }
    SDK::UWorld* RenderWorld() const noexcept { return RenderSnapshot().world; }

    PlayerWorld RenderPlayerWorld() const noexcept {
        auto snapshot = RenderSnapshot();
        return {snapshot.world, snapshot.player};
    }

public:
    Section(ModContext& ctx, const SectionDefinition& definition) noexcept : ctx(ctx), sectionDefinition(&definition) {}
    Section(ModContext&, SectionDefinition&&) = delete;
    virtual ~Section() = default;

    virtual void Render() = 0;
    virtual void OnOpen() {}
    virtual KeybindList* GetSearchKeybinds() noexcept { return nullptr; }

    MenuTab GetTab() const noexcept { return sectionDefinition->tab; }
    const char* GetName() const noexcept { return sectionDefinition->name; }
    const char* GetDescription() const noexcept { return sectionDefinition->description; }

    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
};
