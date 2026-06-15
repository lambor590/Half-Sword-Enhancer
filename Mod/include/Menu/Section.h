#pragma once

#include "Core/ModContext.h"
#include "Menu/MenuTab.h"

struct SectionDefinition {
    MenuTab tab;
    const char* name;
    const char* group = nullptr;
};

class Section {
protected:
    ModContext& ctx;
    const SectionDefinition definition;

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
    Section(ModContext& ctx, SectionDefinition definition) noexcept : ctx(ctx), definition(definition) {}
    virtual ~Section() = default;

    virtual void Render() = 0;
    virtual void OnOpen() {}

    const char* GetName() const noexcept { return definition.name; }
    const char* GetGroup() const noexcept { return definition.group; }

    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
};
