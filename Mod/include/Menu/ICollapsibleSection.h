#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

#include "imgui/imgui.h"
#include "Menu/IMenuFunction.h"
#include "GameInstances.h"
#include "Hooks/GameHook.h"

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
    virtual const std::string& GetName() const = 0;
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
    explicit CollapsibleSection(const std::string& name) : name(name) {}
    
    void Render() override;
    const std::string& GetName() const override { return name; }
    
    void AddFunction(std::unique_ptr<IMenuFunction> function);
    
    struct FunctionBuilder {
        CollapsibleSection* section;
        std::string name;
        int* keyPtr = nullptr;
        std::vector<GameHook::GameEvent> eventTypes;
        bool toggleable = false;
        std::optional<std::initializer_list<Parameter>> paramsList;

        template<typename... Components>
        FunctionBuilder WithKey(int* key) const { FunctionBuilder fb = *this; fb.keyPtr = key; return fb; }
        FunctionBuilder OnEvent(GameHook::GameEvent evt) const { FunctionBuilder fb = *this; fb.eventTypes.push_back(evt); return fb; }
        FunctionBuilder Toggle(bool t = true) const { FunctionBuilder fb = *this; fb.toggleable = t; return fb; }
        FunctionBuilder WithParams(std::initializer_list<Parameter> p) const { FunctionBuilder fb = *this; fb.paramsList = p; return fb; }
        
        template<typename Callback, typename... Components>
        void Action(Callback callback, Components*&... comps) {
            auto sec = section;
            auto validatedCb = [sec, callback = std::move(callback), &comps...](bool active) {
                if (!(... && sec->instances.ValidateComponent(comps))) return;
                if constexpr (std::is_invocable_v<Callback>) callback(); else callback(active);
            };

            if (!eventTypes.empty()) {
                auto fn = std::make_unique<HookedFunction>(name, eventTypes, validatedCb, keyPtr, toggleable);
                section->AddFunctionWithParams(std::move(fn), paramsList.value_or(std::initializer_list<Parameter>{}));
            } else {
                auto fn = std::make_unique<KeybindFunction>(name, keyPtr, validatedCb, toggleable);
                section->AddFunctionWithParams(std::move(fn), paramsList.value_or(std::initializer_list<Parameter>{}));
            }
        }
    };

    FunctionBuilder Function(const std::string& funcName) {
        return FunctionBuilder{this, funcName};
    }

    template<typename T>
    void AddFunctionWithParams(std::unique_ptr<T> function, 
                              const std::initializer_list<Parameter>& params) {
        for (const auto& param : params) {
            function->AddParameter(param);
        }

        function->LoadParameters();
        AddFunction(std::move(function));
    }
};