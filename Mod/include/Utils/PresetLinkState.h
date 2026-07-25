#pragma once

#include <filesystem>
#include <string>
#include <utility>

#include "Utils/PresetDataBase.h"
#include "Utils/PresetLinkResolution.h"

template <typename Serializer> class PresetLinkState {
public:
    using Data = typename Serializer::Data;
    using Link = PresetLink<Data>;
    using ResolveResult = PresetResolveResult<Data>;

    PresetLinkState() = default;
    [[nodiscard]] ResolveResult AssignAndResolve(Link loadedLink, const std::filesystem::path& appDataRoot) {
        link = std::move(loadedLink);
        broken = false;
        diagnostic.clear();
        resolvedPath.clear();
        return Resolve(appDataRoot);
    }

    [[nodiscard]] ResolveResult Resolve(const std::filesystem::path& appDataRoot) {
        auto result = PresetLinkResolution::Resolve<Serializer>(link, appDataRoot);
        broken = !result.success && HasLink();
        diagnostic = PresetLinkResolution::FormatDiagnostic(result);
        resolvedPath = result.path;
        return result;
    }

    void MarkBroken(std::string detail) {
        broken = HasLink();
        diagnostic = std::move(detail);
    }

    void MarkHealthy() {
        broken = false;
        diagnostic.clear();
    }

    void Clear() {
        link = std::monostate{};
        broken = false;
        diagnostic.clear();
        resolvedPath.clear();
    }

    [[nodiscard]] bool HasLink() const noexcept { return !IsEmptyPresetLink(link); }
    [[nodiscard]] bool IsBroken() const noexcept { return broken; }
    [[nodiscard]] const Link& GetLink() const noexcept { return link; }
    [[nodiscard]] const std::string& GetDiagnostic() const noexcept { return diagnostic; }
    [[nodiscard]] const std::filesystem::path& GetResolvedPath() const noexcept { return resolvedPath; }

private:
    Link link;
    bool broken = false;
    std::string diagnostic;
    std::filesystem::path resolvedPath;
};
