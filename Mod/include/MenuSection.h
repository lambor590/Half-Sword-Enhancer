#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Action.h"
#include "imgui.h"

// Forward declaration
class Gui;

struct MenuItem {
    std::string id;
    std::string name;
    std::string description;
    bool isCollapsible = true;
    bool isCollapsed = false;
};

class MenuSection {
public:
    MenuSection(const std::string& id, const std::string& title) 
        : id(id), title(title) {}

    const std::string& GetTitle() const { return title; }

    void AddItem(const MenuItem& item, const Action& action) {
        items.push_back(item);
        ActionManager::Get().RegisterAction(item.id, action);
    }

    void Render();

private:
    void RenderItemContent(const MenuItem& item);

    std::string id;
    std::string title;
    std::vector<MenuItem> items;
};

class MenuManager {
public:
    static MenuManager& Get() {
        static MenuManager instance;
        return instance;
    }

    void SetGuiInstance(Gui* gui) {
        guiInstance = gui;
    }

    Gui* GetGuiInstance() const {
        return guiInstance;
    }

    MenuSection& CreateSection(const std::string& id, const std::string& title) {
        sections.push_back(std::make_unique<MenuSection>(id, title));
        return *sections.back();
    }

    void RenderAll() {
        if (!ImGui::BeginTabBar("MainTabBar")) return;

        for (auto& section : sections) {
            if (ImGui::BeginTabItem(section->GetTitle().c_str())) {
                section->Render();
                ImGui::EndTabItem();
            }
        }

        ImGui::EndTabBar();
    }

private:
    std::vector<std::unique_ptr<MenuSection>> sections;
    Gui* guiInstance = nullptr;
};