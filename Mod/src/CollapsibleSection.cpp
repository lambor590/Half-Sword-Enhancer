#include <iostream>

#include "Menu/IMenuFunction.h"
#include "Menu/ICollapsibleSection.h"

void CollapsibleSection::Render() {
    if (ImGui::CollapsingHeader(name.c_str())) {
        for (auto& function : functions) {
            function->Render();
        }
    }
}

void CollapsibleSection::AddFunction(std::unique_ptr<IMenuFunction> function) {    
    if (auto hookedFunction = dynamic_cast<HookedFunction*>(function.get())) {
        hookedFunction->LoadConfig();
    } else if (auto keybindFunction = dynamic_cast<KeybindFunction*>(function.get())) {
        keybindFunction->LoadConfig();
    }
    
    functions.push_back(std::move(function));
}