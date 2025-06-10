#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui/imgui.h"
#include "Menu/IMenuFunction.h"
#include "GameInstances.h"
#include "Hooks/GameHook.h"

namespace SectionStyle {
    constexpr ImVec2 framePadding{8, 6};
    constexpr ImVec2 itemSpacing{10, 8};
    constexpr float indentSpacing = 25.0f;
    constexpr float indentAmount = 10.0f;
    
    struct StyleRAII {
        StyleRAII() noexcept {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing);
            ImGui::Indent(indentAmount);
            ImGui::Spacing();
        }
        ~StyleRAII() noexcept {
            ImGui::Unindent(indentAmount);
            ImGui::PopStyleVar(3);
        }
        StyleRAII(const StyleRAII&) = delete;
        StyleRAII& operator=(const StyleRAII&) = delete;
    };
}

class ICollapsibleSection {
protected:
    GameInstances& instances = GameInstances::Get();

    SDK::UWorld* world = nullptr;
    SDK::APlayerController* controller = nullptr;
    SDK::AWillie_BP_C* player = nullptr;
    SDK::AWorldSettings* worldSettings = nullptr;

public:
    virtual ~ICollapsibleSection() = default;
    virtual void Render() = 0;
    virtual const std::string& GetName() const noexcept = 0;
};

class CollapsibleSection : public ICollapsibleSection {
protected:
    std::string name;
    std::vector<std::unique_ptr<IMenuFunction>> functions;
    
    template<typename... Components>
    std::function<void()> ValidateAndRun(const std::function<void()>& callback, Components*&... components) {
        return [this, callback, &components...]() {
            bool valid = (... && instances.ValidateComponent(components));
            if (valid) callback();
        };
    }

public:
    explicit CollapsibleSection(std::string name) noexcept : name(std::move(name)) {}
    
    void Render() override;
    const std::string& GetName() const noexcept override { return name; }
    
    void AddFunction(std::unique_ptr<IMenuFunction> function) {
        functions.emplace_back(std::move(function));
    }
    
    struct FunctionBuilder {
        CollapsibleSection* section;
        std::string name;
        std::string tooltip;
        int* keyPtr = nullptr;
        std::vector<GameHook::GameEvent> eventTypes;
        bool toggleable = false;
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
        
        FunctionBuilder&& WithParams(std::initializer_list<Parameter> p) && noexcept { 
            params.assign(p);
            return std::move(*this); 
        }
        
        FunctionBuilder&& WithTooltip(std::string_view tip) && noexcept {
            tooltip = tip;
            return std::move(*this);
        }

        template<typename Callback, typename... Components>
        void Action(Callback&& callback, Components*&... comps) && {
            auto validatedCb = [this, callback = std::forward<Callback>(callback), &comps...](bool active) {
                if (!(... && section->instances.ValidateComponent(comps))) return;
                if constexpr (std::is_invocable_v<Callback>) {
                    callback();
                } else {
                    callback(active);
                }
            };

            std::unique_ptr<IMenuFunction> fn;
            if (eventTypes.empty()) {
                fn = std::make_unique<KeybindFunction>(
                    std::move(name), keyPtr, std::move(validatedCb), toggleable, std::move(tooltip)
                );
            } else {
                fn = std::make_unique<HookedFunction>(
                    std::move(name), std::move(eventTypes), std::move(validatedCb), keyPtr, toggleable, std::move(tooltip)
                );
            }
            
            for (const auto& param : params) {
                fn->AddParameter(param);
            }
            fn->LoadParameters();
            section->AddFunction(std::move(fn));
        }
    };

    FunctionBuilder Function(std::string funcName) {
        return FunctionBuilder{this, std::move(funcName)};
    }
};