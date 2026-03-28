#pragma once

#include <string>

#include "Core/ModContext.h"

/// Flat base class for all menu sections.
/// Each section owns its rendering via the pure virtual Render() method.
/// Sections receive ModContext in the constructor for access to cached game state.
/// Convenience pointer references (world, player, controller, worldSettings) allow
/// sections to use `player->X` instead of `ctx.player->X` for backward compatibility.
class Section {
protected:
    ModContext& ctx;
    std::string name;

    /// Convenience references to ModContext cached pointers.
    /// Always in sync because they reference the ModContext members directly.
    SDK::UWorld*& world;
    SDK::AWillie_BP_C*& player;
    SDK::APlayerController*& controller;
    SDK::AWorldSettings*& worldSettings;

public:
    Section(ModContext& ctx, std::string name) noexcept
        : ctx(ctx),
          name(std::move(name)),
          world(ctx.world),
          player(ctx.player),
          controller(ctx.controller),
          worldSettings(ctx.worldSettings) {}
    virtual ~Section() = default;

    virtual void Render() = 0;

    const std::string& GetName() const noexcept { return name; }
    virtual const char* GetGroup() const noexcept { return nullptr; }

    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
};
