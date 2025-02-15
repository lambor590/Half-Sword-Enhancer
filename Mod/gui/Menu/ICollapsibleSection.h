#pragma once
#include <string>
#include <vector>
#include <memory>
#include "imgui.h"
#include "IMenuFunction.h"
#include "GameInstances.h"

class ICollapsibleSection {
protected:
    // Referencias directas a GameInstances
    GameInstances& instances = GameInstances::Get();
    // Accesos directos para conveniencia
    SDK::UWorld*& world = instances.GetWorld();
    SDK::APlayerController*& controller = instances.GetPlayerController();
    SDK::APawn*& player = instances.GetPlayerPawn();

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
}; 