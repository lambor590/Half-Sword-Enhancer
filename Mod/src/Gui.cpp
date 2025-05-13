#include "Gui.h"
#include "KeybindManager.h"

Gui* Gui::s_instance = nullptr;
WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

void MedievalStyle::PushButtonStyle() {
    ImGui::PushStyleColor(ImGuiCol_Button, darkWood);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, lightWood);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, oldBrass);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
}

void MedievalStyle::PopButtonStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void MedievalStyle::PushCheckboxStyle() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, darkWood);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, mediumWood);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, darkLeather);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, brightBrass);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
}

void MedievalStyle::PopCheckboxStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
}

void MedievalStyle::PushInputStyle() {
    ImGui::PushStyleColor(ImGuiCol_FrameBg, darkWood);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, mediumWood);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, darkLeather);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
}

void MedievalStyle::PopInputStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void MedievalStyle::PushHeaderStyle() {
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.28f, 0.19f, 0.11f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.38f, 0.26f, 0.15f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.44f, 0.30f, 0.18f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 10));
}

void MedievalStyle::PopHeaderStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void MedievalStyle::PushPopupStyle() {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.12f, 0.09f, 0.06f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
}

void MedievalStyle::PopPopupStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void MedievalStyle::ApplyGlobalStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    colors[ImGuiCol_WindowBg] = black;
    colors[ImGuiCol_Border] = oldBrass;
    colors[ImGuiCol_BorderShadow] = shadow;
    colors[ImGuiCol_Text] = parchment;
    colors[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.60f, 0.55f, 1.00f);
    
    colors[ImGuiCol_Header] = darkWood;
    colors[ImGuiCol_HeaderHovered] = mediumWood;
    colors[ImGuiCol_HeaderActive] = darkLeather;
    
    colors[ImGuiCol_Tab] = darkWood;
    colors[ImGuiCol_TabHovered] = mediumWood;
    colors[ImGuiCol_TabActive] = lightWood;
    colors[ImGuiCol_TabUnfocused] = darkInk;
    colors[ImGuiCol_TabUnfocusedActive] = darkWood;
    
    colors[ImGuiCol_Button] = darkWood;
    colors[ImGuiCol_ButtonHovered] = lightWood;
    colors[ImGuiCol_ButtonActive] = oldBrass;
    
    colors[ImGuiCol_FrameBg] = darkWood;
    colors[ImGuiCol_FrameBgHovered] = mediumWood;
    colors[ImGuiCol_FrameBgActive] = lightWood;
    
    colors[ImGuiCol_ScrollbarBg] = darkInk;
    colors[ImGuiCol_ScrollbarGrab] = darkLeather;
    colors[ImGuiCol_ScrollbarGrabHovered] = mediumWood;
    colors[ImGuiCol_ScrollbarGrabActive] = oldBrass;
    
    colors[ImGuiCol_TitleBg] = darkWood;
    colors[ImGuiCol_TitleBgActive] = mediumWood;
    colors[ImGuiCol_TitleBgCollapsed] = darkInk;
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.09f, 0.06f, 0.98f);
    
    colors[ImGuiCol_CheckMark] = brightBrass;
    colors[ImGuiCol_SliderGrab] = darkLeather;
    colors[ImGuiCol_SliderGrabActive] = oldBrass;
    colors[ImGuiCol_ResizeGrip] = darkLeather;
    colors[ImGuiCol_ResizeGripHovered] = mediumWood;
    colors[ImGuiCol_ResizeGripActive] = oldBrass;
    colors[ImGuiCol_Separator] = oldBrass;
    colors[ImGuiCol_SeparatorHovered] = mediumWood;
    colors[ImGuiCol_SeparatorActive] = brightBrass;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.70f, 0.55f, 0.30f, 0.35f);
    
    style.WindowPadding = ImVec2(18, 18);
    style.FramePadding = ImVec2(10, 8);
    style.ItemSpacing = ImVec2(12, 10);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 8.0f;
    
    style.WindowBorderSize = 1.5f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.TabBorderSize = 1.0f;
    
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.5f, 0.5f);
    style.DisplayWindowPadding = ImVec2(18, 18);
    style.DisplaySafeAreaPadding = ImVec2(3, 3);
    
    style.IndentSpacing = 25.0f;
    style.SeparatorTextPadding = ImVec2(15, 6);
}

LRESULT CALLBACK Gui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (KeybindManager::ProcessKeyEvent(msg, wParam))
        return true;
    
    if (msg == WM_KEYDOWN && wParam == KeybindManager::GetToggleGuiKey()) {
        isVisible = !isVisible;
        return true;
    }

    if (isVisible && (
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) || 
        (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && ImGui::GetIO().WantCaptureMouse)
    )) {
        return true;
    }

    return CallWindowProc(originalWndProc, hWnd, msg, wParam, lParam);
}

void Gui::SetupStyle() {
    MedievalStyle::ApplyGlobalStyle();
}

void Gui::Setup() {    
    originalWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
    logger.Log("WndProc hooked successfully");

    ImGui::CreateContext();
    IMGUI_CHECKVERSION();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = _strdup((ConfigManager::GetAppDataPath() / "imgui.ini").string().c_str());

    SetupStyle();

    ImGui::SetNextWindowSize(ImVec2(699, 389), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(544, 331), ImGuiCond_Once, ImVec2(0.5f, 0.5f));

    MenuManager::Get().AddSection<PlayerSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<WorldSection>(MenuTab::Gameplay);
    MenuManager::Get().AddSection<NPCSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<ItemSection>(MenuTab::Entity_Spawner);
    MenuManager::Get().AddSection<GuiSection>(MenuTab::Settings);

    s_instance = this;
}

void Gui::Render() {
    ImGui_ImplWin32_NewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Half Sword Enhancer", &isVisible, ImGuiWindowFlags_NoCollapse);

    MenuManager::Get().RenderMenu();

    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}