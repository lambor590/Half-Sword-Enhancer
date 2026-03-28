#pragma once

/// Transitional bridge: extends Section with FunctionBuilder and functions vector.
/// Sections still using FunctionBuilder inherit from this until the keybind-system
/// feature replaces FunctionBuilder with a lightweight keybind registration system.
/// Once all sections are migrated, this file will be deleted.

#include <vector>
#include <memory>
#include <functional>

#include "imgui/imgui.h"
#include "Menu/Section.h"
#include "Menu/IMenuFunction.h"
#include "ComponentValidator.h"
#include "Hooks/GameHook.h"

namespace SectionStyle {
    constexpr ImVec2 framePadding{8, 6};
    constexpr ImVec2 itemSpacing{10, 8};
    constexpr ImVec2 cellPadding{4, itemSpacing.y * 0.5f};
    constexpr float indentSpacing = 25.0f;

    struct StyleRAII {
        StyleRAII() noexcept {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cellPadding);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing);
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.25f);
        }
        ~StyleRAII() noexcept {
            ImGui::PopItemWidth();
            ImGui::PopStyleVar(4);
        }
        StyleRAII(const StyleRAII&) = delete;
        StyleRAII& operator=(const StyleRAII&) = delete;
    };
}

class CollapsibleSection : public Section {
protected:
    std::vector<std::unique_ptr<IMenuFunction>> functions;

public:
    explicit CollapsibleSection(ModContext& ctx, std::string name) noexcept : Section(ctx, std::move(name)) {}

    void Render() override {
        const SectionStyle::StyleRAII style;
        const size_t count = functions.size();
        for (size_t i = 0; i < count; ++i) {
            functions[i]->Render();
            if (i + 1 < count) {
                ImGui::Spacing();
            }
        }
    }

    const std::vector<std::unique_ptr<IMenuFunction>>& GetFunctions() const noexcept { return functions; }

    void AddFunction(std::unique_ptr<IMenuFunction> function) { functions.emplace_back(std::move(function)); }

    struct FunctionBuilder {
        CollapsibleSection* section;
        std::string name;
        std::string tooltip;
        int* keyPtr = nullptr;
        std::vector<GameHook::GameEvent> eventTypes;
        bool toggleable = false;
        bool runOnGameThread = false;
        std::vector<Parameter> params;

        FunctionBuilder&& WithKey(int* key) && noexcept {
            keyPtr = key;
            return std::move(*this);
        }

        FunctionBuilder&& OnEvent(GameHook::GameEvent evt) && {
            eventTypes.emplace_back(evt);
            return std::move(*this);
        }

        FunctionBuilder&& Toggle(bool t = true) && noexcept {
            toggleable = t;
            return std::move(*this);
        }

        FunctionBuilder&& GameThread() && noexcept {
            runOnGameThread = true;
            return std::move(*this);
        }

        FunctionBuilder&& WithParams(std::initializer_list<Parameter> p) && noexcept {
            params.assign(p);
            return std::move(*this);
        }

        FunctionBuilder&& WithTooltip(std::string_view tip) && noexcept {
            tooltip = tip;
            return std::move(*this);
        }

        template <typename Callback, typename... Components>
        void Action(Callback&& callback, Components*&... comps) && {
            auto validatedCb = [this, callback = std::forward<Callback>(callback),
                                &comps...]([[maybe_unused]] bool active) {
                if (!(... && ComponentValidator::Validate(comps))) return;
                if constexpr (std::is_invocable_v<Callback>) {
                    callback();
                } else {
                    callback(active);
                }
            };

            std::function<void(bool)> finalCb;
            if (runOnGameThread) {
                finalCb = [validatedCb = std::move(validatedCb)](bool active) {
                    GameHook::QueueAction([validatedCb, active]() { validatedCb(active); });
                };
            } else {
                finalCb = std::move(validatedCb);
            }

            std::unique_ptr<IMenuFunction> fn;
            if (eventTypes.empty()) {
                fn = std::make_unique<KeybindFunction>(
                    std::move(name), keyPtr, std::move(finalCb), toggleable, std::move(tooltip)
                );
            } else {
                fn = std::make_unique<HookedFunction>(
                    std::move(name), std::move(eventTypes), std::move(finalCb), keyPtr, toggleable, std::move(tooltip)
                );
            }

            for (const auto& param : params) {
                fn->AddParameter(param);
            }
            fn->LoadParameters();
            section->AddFunction(std::move(fn));
        }
    };

    FunctionBuilder Function(std::string funcName) { return FunctionBuilder{this, std::move(funcName)}; }
};
