#include "Gui.h"

Gui* Gui::s_instance = nullptr;
WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;
std::unordered_map<int*, std::function<void()>> Gui::s_keybinds;

Logger logger("Gui");

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (GetAsyncKeyState(VK_INSERT) & 1)
        isVisible = !isVisible;

    if (msg == WM_KEYDOWN) {
        for (const auto& [key, callback] : s_keybinds) {
            if (*key == wParam)
                callback();
        }
    }

    if (isVisible && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::SetupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    ImVec4* colors = style.Colors;
    const ImVec4 gold = ImVec4(0.75f, 0.55f, 0.12f, 1.00f);
    const ImVec4 darkGold = ImVec4(0.55f, 0.42f, 0.10f, 1.00f);
    const ImVec4 darkBrown = ImVec4(0.20f, 0.15f, 0.10f, 1.00f);
    const ImVec4 lightBrown = ImVec4(0.35f, 0.25f, 0.16f, 1.00f);
    const ImVec4 black = ImVec4(0.08f, 0.07f, 0.06f, 0.95f);
    const ImVec4 cream = ImVec4(0.92f, 0.88f, 0.83f, 1.00f);
    const ImVec4 darkCream = ImVec4(0.82f, 0.78f, 0.73f, 1.00f);

    colors[ImGuiCol_WindowBg] = black;
    colors[ImGuiCol_Border] = gold;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
    colors[ImGuiCol_Text] = cream;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);

    colors[ImGuiCol_Header] = darkBrown;
    colors[ImGuiCol_HeaderHovered] = lightBrown;
    colors[ImGuiCol_HeaderActive] = darkGold;
    
    colors[ImGuiCol_Button] = darkBrown;
    colors[ImGuiCol_ButtonHovered] = lightBrown;
    colors[ImGuiCol_ButtonActive] = darkGold;
    
    colors[ImGuiCol_FrameBg] = darkBrown;
    colors[ImGuiCol_FrameBgHovered] = lightBrown;
    colors[ImGuiCol_FrameBgActive] = darkGold;

    colors[ImGuiCol_Tab] = darkBrown;
    colors[ImGuiCol_TabHovered] = lightBrown;
    colors[ImGuiCol_TabActive] = darkGold;
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.12f, 0.09f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = lightBrown;

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.10f, 0.08f, 0.90f);
    colors[ImGuiCol_ScrollbarGrab] = darkBrown;
    colors[ImGuiCol_ScrollbarGrabHovered] = lightBrown;
    colors[ImGuiCol_ScrollbarGrabActive] = gold;

    colors[ImGuiCol_TitleBg] = darkBrown;
    colors[ImGuiCol_TitleBgActive] = darkGold;
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.12f, 0.09f, 0.90f);
    
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.5f, 0.5f);
    style.DisplayWindowPadding = ImVec2(15, 15);
    style.DisplaySafeAreaPadding = ImVec2(2, 2);

    colors[ImGuiCol_CheckMark] = gold;
    
    colors[ImGuiCol_ResizeGrip] = darkBrown;
    colors[ImGuiCol_ResizeGripHovered] = lightBrown;
    colors[ImGuiCol_ResizeGripActive] = gold;

    colors[ImGuiCol_SliderGrab] = lightBrown;
    colors[ImGuiCol_SliderGrabActive] = gold;
    
    colors[ImGuiCol_Separator] = darkGold;
    colors[ImGuiCol_SeparatorHovered] = lightBrown;
    colors[ImGuiCol_SeparatorActive] = gold;

    colors[ImGuiCol_Text] = cream;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.75f, 0.55f, 0.12f, 0.35f);
}

void Gui::Setup() {
    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");

    ImGui::CreateContext();
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = NULL;

    SetupStyle();

    MenuManager::Get().AddSection<GeneralSection>(MenuTab::Gameplay);

    s_instance = this;
}

void Gui::Render() {
    if (!isVisible)
        return;

    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(699, 389), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(544, 331), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15, 15));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 8));
    
    ImGui::Begin("Half Sword Enhancer", &isVisible, ImGuiWindowFlags_NoCollapse);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable)) {
        if (ImGui::BeginTabItem("Gameplay")) {
            MenuManager::Get().RenderSections(MenuTab::Gameplay);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Item Spawner")) {
            ImGui::Text("Coming Soon");
            // MenuManager::Get().RenderSections(MenuTab::Item_Spawner);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Loadout Manager")) {
            ImGui::Text("Coming Soon");
            // MenuManager::Get().RenderSections(MenuTab::Loadout_Manager);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Post Process")) {
            ImGui::Text("Coming Soon");
            // MenuManager::Get().RenderSections(MenuTab::Post_Process_Settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Text("Coming Soon");
            // MenuManager::Get().RenderSections(MenuTab::Settings);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::PopStyleVar(3);

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}