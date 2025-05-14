#include "Gui.h"
#include "KeybindManager.h"

WNDPROC Gui::originalWndProc = nullptr;
bool Gui::isVisible = true;

Logger logger("Gui");

void MedievalStyle::PushButtonStyle() {
    static const ImGuiCol colorIds[] = {
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive, ImGuiCol_Border
    };
    static const ImVec4 colorValues[] = {
        darkWood, lightWood, oldBrass, ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.90f)
    };
    
    for (int i = 0; i < IM_ARRAYSIZE(colorIds); i++) {
        ImGui::PushStyleColor(colorIds[i], colorValues[i]);
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
}

void MedievalStyle::PopButtonStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void MedievalStyle::PushCheckboxStyle() {
    static const ImGuiCol colorIds[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive, 
        ImGuiCol_CheckMark, ImGuiCol_Border
    };
    static const ImVec4 colorValues[] = {
        darkWood, mediumWood, darkLeather, brightBrass, 
        ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f)
    };
    
    for (int i = 0; i < IM_ARRAYSIZE(colorIds); i++) {
        ImGui::PushStyleColor(colorIds[i], colorValues[i]);
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
}

void MedievalStyle::PopCheckboxStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);
}

void MedievalStyle::PushInputStyle() {
    static const ImGuiCol colorIds[] = {
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive, ImGuiCol_Border
    };
    static const ImVec4 colorValues[] = {
        darkWood, mediumWood, darkLeather, 
        ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f)
    };
    
    for (int i = 0; i < IM_ARRAYSIZE(colorIds); i++) {
        ImGui::PushStyleColor(colorIds[i], colorValues[i]);
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
}

void MedievalStyle::PopInputStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void MedievalStyle::PushHeaderStyle() {
    static const ImGuiCol colorIds[] = {
        ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive, ImGuiCol_Border
    };
    static const ImVec4 colorValues[] = {
        ImVec4(0.28f, 0.19f, 0.11f, 1.00f), 
        ImVec4(0.38f, 0.26f, 0.15f, 1.00f), 
        ImVec4(0.44f, 0.30f, 0.18f, 1.00f),
        ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f)
    };
    
    for (int i = 0; i < IM_ARRAYSIZE(colorIds); i++) {
        ImGui::PushStyleColor(colorIds[i], colorValues[i]);
    }
    
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 10));
}

void MedievalStyle::PopHeaderStyle() {
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
}

void MedievalStyle::PushPopupStyle() {
    static const ImGuiCol colorIds[] = {
        ImGuiCol_PopupBg, ImGuiCol_Border
    };
    static const ImVec4 colorValues[] = {
        ImVec4(0.12f, 0.09f, 0.06f, 0.98f),
        ImVec4(oldBrass.x, oldBrass.y, oldBrass.z, 0.80f)
    };
    
    for (int i = 0; i < IM_ARRAYSIZE(colorIds); i++) {
        ImGui::PushStyleColor(colorIds[i], colorValues[i]);
    }
    
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
    
    static const ImGuiCol basicColorIds[] = {
        ImGuiCol_WindowBg, ImGuiCol_Border, ImGuiCol_BorderShadow, 
        ImGuiCol_Text, ImGuiCol_TextDisabled,
        ImGuiCol_Header, ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive,
        ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabActive, 
        ImGuiCol_TabUnfocused, ImGuiCol_TabUnfocusedActive,
        ImGuiCol_Button, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive,
        ImGuiCol_FrameBg, ImGuiCol_FrameBgHovered, ImGuiCol_FrameBgActive,
        ImGuiCol_ScrollbarBg, ImGuiCol_ScrollbarGrab, 
        ImGuiCol_ScrollbarGrabHovered, ImGuiCol_ScrollbarGrabActive,
        ImGuiCol_TitleBg, ImGuiCol_TitleBgActive, ImGuiCol_TitleBgCollapsed,
        ImGuiCol_CheckMark, ImGuiCol_SliderGrab, ImGuiCol_SliderGrabActive,
        ImGuiCol_ResizeGrip, ImGuiCol_ResizeGripHovered, ImGuiCol_ResizeGripActive,
        ImGuiCol_Separator, ImGuiCol_SeparatorHovered, ImGuiCol_SeparatorActive,
        ImGuiCol_TextSelectedBg
    };
    
    static const ImVec4 basicColorValues[] = {
        black, oldBrass, shadow, parchment, ImVec4(0.65f, 0.60f, 0.55f, 1.00f),
        darkWood, mediumWood, darkLeather,
        darkWood, mediumWood, lightWood, darkInk, darkWood,
        darkWood, lightWood, oldBrass,
        darkWood, mediumWood, lightWood,
        darkInk, darkLeather, mediumWood, oldBrass,
        darkWood, mediumWood, darkInk,
        brightBrass, darkLeather, oldBrass,
        darkLeather, mediumWood, oldBrass,
        oldBrass, mediumWood, brightBrass,
        ImVec4(0.70f, 0.55f, 0.30f, 0.35f)
    };
    
    for (int i = 0; i < IM_ARRAYSIZE(basicColorIds); i++) {
        colors[basicColorIds[i]] = basicColorValues[i];
    }
    
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.09f, 0.06f, 0.98f);
    
    struct StyleProperty {
        enum Type { Float, Vec2 };
        Type type;
        size_t offset;
        union Value {
            float floatVal;
            ImVec2 vec2Val;
            Value() : floatVal(0) {}
            Value(float val) : floatVal(val) {}
            Value(const ImVec2& val) : vec2Val(val) {}
        } value;
        
        StyleProperty(Type t, size_t off, float val) : type(t), offset(off), value(val) {}
        StyleProperty(Type t, size_t off, const ImVec2& val) : type(t), offset(off), value(val) {}
    };
    
    static const StyleProperty styleProps[] = {
        { StyleProperty::Vec2, offsetof(ImGuiStyle, WindowPadding), ImVec2(18, 18) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, FramePadding), ImVec2(10, 8) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, ItemSpacing), ImVec2(12, 10) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, ItemInnerSpacing), ImVec2(8, 6) },
        { StyleProperty::Float, offsetof(ImGuiStyle, WindowRounding), 8.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, ChildRounding), 6.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, FrameRounding), 5.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, PopupRounding), 6.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, ScrollbarRounding), 6.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, GrabRounding), 5.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, TabRounding), 8.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, WindowBorderSize), 1.5f },
        { StyleProperty::Float, offsetof(ImGuiStyle, FrameBorderSize), 1.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, PopupBorderSize), 1.0f },
        { StyleProperty::Float, offsetof(ImGuiStyle, TabBorderSize), 1.0f },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, WindowTitleAlign), ImVec2(0.5f, 0.5f) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, ButtonTextAlign), ImVec2(0.5f, 0.5f) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, SelectableTextAlign), ImVec2(0.5f, 0.5f) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, DisplayWindowPadding), ImVec2(18, 18) },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, DisplaySafeAreaPadding), ImVec2(3, 3) },
        { StyleProperty::Float, offsetof(ImGuiStyle, IndentSpacing), 25.0f },
        { StyleProperty::Vec2, offsetof(ImGuiStyle, SeparatorTextPadding), ImVec2(15, 6) }
    };
    
    for (const auto& prop : styleProps) {
        if (prop.type == StyleProperty::Float) {
            *reinterpret_cast<float*>(reinterpret_cast<char*>(&style) + prop.offset) = prop.value.floatVal;
        } else {
            *reinterpret_cast<ImVec2*>(reinterpret_cast<char*>(&style) + prop.offset) = prop.value.vec2Val;
        }
    }
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