#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "imgui/imgui.h"
#include "IMenuFunction.h"
#include "GameInstances.h"

class ICollapsibleSection {
protected:
    GameInstances& instances = GameInstances::Get();

    SDK::UWorld* world = nullptr;
    SDK::APlayerController* controller = nullptr;
    SDK::AWillie_BP_C* player = nullptr;
    SDK::AWorldSettings* worldSettings = nullptr;

    template<typename... Components>
    auto ValidateAndRun(std::function<void()> callback, Components*&... components) {
        return [this, callback, &components...]() {
            if ((instances.ValidateComponent(components) && ...)) {
                callback();
            }
        };
    }

public:
    virtual ~ICollapsibleSection() = default;
    virtual void Render() = 0;
    virtual const std::string& GetName() const = 0;
    virtual void AddFunction(std::unique_ptr<IMenuFunction> function) = 0;
};

class CollapsibleSection : public ICollapsibleSection {
private:
    std::string name;
    std::vector<std::unique_ptr<IMenuFunction>> functions;

public:
    CollapsibleSection(const std::string& name) : name(name) {}

    void Render() override {
        if (ImGui::CollapsingHeader(name.c_str())) {
            for (auto& function : functions) {
                function->Render();
            }
        }
    }

    const std::string& GetName() const override { return name; }

    void AddFunction(std::unique_ptr<IMenuFunction> function) override {
        functions.push_back(std::move(function));
    }

    template<typename... Components>
    void Hook(const std::string& name, const std::string& hookedFunction, std::function<void()> callback, Components*&... components) {
        AddFunction(std::make_unique<HookedFunction>(name, hookedFunction, ValidateAndRun(callback, components...)));
    }

    template<typename... Components>
    void Bind(const std::string& name, int* key, std::function<void()> callback, Components*&... components) {
        AddFunction(std::make_unique<KeybindFunction>(name, key, ValidateAndRun(callback, components...)));
    }
}; 