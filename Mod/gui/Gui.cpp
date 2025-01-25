#include "Gui.h"

Gui* Gui::s_instance = nullptr;
WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto& instance = *s_instance;

    if (GetAsyncKeyState(VK_INSERT) & 1)
        isVisible = !isVisible;

    // Capturar teclas para keybindings
    if (instance.waitingForKey && msg == WM_KEYDOWN) {
        instance.waitingForKey = false;
        ActionManager::Get().SetKeyBinding(instance.pendingKeybindActionId, (int)wParam);
        return true;
    }

    if (isVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::Setup() {
    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");

    ImGui::CreateContext();
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = "HS-Enhancer-imgui.ini";

    ImGui::StyleColorsDark();
    RegisterActions();

    s_instance = this;
}

void Gui::RegisterActions() {
    auto& actions = ActionManager::Get();

    // Combat actions
    actions.RegisterAction("combat.autoblock", {
        "Auto Block",
        false,
        VK_F1,
        []() { /* Implementar auto block */ }
    });

    actions.RegisterAction("combat.parry", {
        "Auto Parry",
        false,
        VK_F2,
        []() { /* Implementar auto parry */ }
    });

    // Movement actions
    actions.RegisterAction("movement.speed", {
        "Speed Control",
        false,
        VK_F3,
        []() { /* Implementar speed control */ }
    });
}

void Gui::RenderActionToggle(const std::string& id) {
    if (Action* action = ActionManager::Get().GetAction(id)) {
        ImGui::Checkbox(action->name.c_str(), &action->enabled);
    }
}

void Gui::RenderKeybind(const std::string& id) {
    if (Action* action = ActionManager::Get().GetAction(id)) {
        ImGui::SameLine();
        char keyName[32];
        sprintf_s(keyName, "[%c]", action->keyBinding ? action->keyBinding : '?');

        if (ImGui::Button(keyName)) {
            waitingForKey = true;
            pendingKeybindActionId = id;
        }
    }
}

void Gui::RenderGameplayTab() {
    if (ImGui::CollapsingHeader("Combat")) {
        ImGui::Indent();
        RenderActionToggle("combat.autoblock");
        RenderKeybind("combat.autoblock");
        
        RenderActionToggle("combat.parry");
        RenderKeybind("combat.parry");
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Movement")) {
        ImGui::Indent();
        RenderActionToggle("movement.speed");
        RenderKeybind("movement.speed");
        ImGui::Unindent();
    }
}

void Gui::RenderVisualTab() {
    // TODO: Implementar visual tab
}

void Gui::RenderDebugTab() {
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("Delta Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
}

void Gui::RenderSettingsTab() {
    // TODO: Implementar settings tab
}

void Gui::Render() {
    if (!isVisible)
        return;

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Half Sword Enhancer", &isVisible,
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("MainTabBar")) {
        if (ImGui::BeginTabItem("Gameplay")) {
            RenderGameplayTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Visual")) {
            RenderVisualTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Debug")) {
            RenderDebugTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            RenderSettingsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}