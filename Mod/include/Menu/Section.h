#pragma once

#include <string>

class ModContext;

/// Flat base class for all menu sections.
/// Each section owns its rendering via the pure virtual Render() method.
/// Sections receive ModContext in the constructor for access to cached game state.
class Section {
protected:
    ModContext& ctx;
    std::string name;

public:
    Section(ModContext& ctx, std::string name) noexcept : ctx(ctx), name(std::move(name)) {}
    virtual ~Section() = default;

    virtual void Render() = 0;

    const std::string& GetName() const noexcept { return name; }
    virtual const char* GetGroup() const noexcept { return nullptr; }

    Section(const Section&) = delete;
    Section& operator=(const Section&) = delete;
};
