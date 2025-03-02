#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui/imgui.h"
#include "Menu/IMenuFunction.h"
#include "GameInstances.h"

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
            if ((instances.ValidateComponent(components) && ...)) {
                callback();
            }
        };
    }

public:
    CollapsibleSection(const std::string& name) : name(name) {}
    
    void Render() override;
    const std::string& GetName() const override { return name; }
    
    void AddFunction(std::unique_ptr<IMenuFunction> function);
    
    template<typename... Components>
    inline void Hook(const std::string& name, const std::string& hookedFunction, 
                    int* key, std::function<void()> callback, Components*&... components) {
        AddFunction(std::make_unique<HookedFunction>(name, hookedFunction, ValidateAndRun(callback, components...), key));
    }
    
    template<typename... Components>
    inline void Bind(const std::string& name, int* key, 
                   std::function<void()> callback, Components*&... components) {
        AddFunction(std::make_unique<KeybindFunction>(name, key, ValidateAndRun(callback, components...)));
    }
}; 