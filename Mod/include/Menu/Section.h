#pragma once

#include <string>

#include "Core/ModContext.h"

class Section {
protected:
    ModContext& ctx;
    std::string name;

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
    Section(ModContext& ctx, std::string name) noexcept : ctx(ctx), name(std::move(name)) {}
    virtual ~Section() = default;

    virtual void Render() = 0;

    const std::string& GetName() const noexcept { return name; }
    virtual const char* GetGroup() const noexcept { return nullptr; }

    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
};
