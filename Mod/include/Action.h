#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <Windows.h>

struct Action {
    std::string name;
    bool enabled = false;
    int keyBinding = 0;
    std::function<void()> execute;
};

class ActionManager {
public:
    static ActionManager& Get() {
        static ActionManager instance;
        return instance;
    }

    void RegisterAction(const std::string& id, const Action& action) {
        actions[id] = action;
    }

    Action* GetAction(const std::string& id) {
        auto it = actions.find(id);
        return it != actions.end() ? &it->second : nullptr;
    }

    void ExecuteAction(const std::string& id) {
        if (auto* action = GetAction(id)) {
            if (action->enabled && action->execute) {
                action->execute();
            }
        }
    }

    void SetKeyBinding(const std::string& id, int key) {
        if (auto* action = GetAction(id)) {
            action->keyBinding = key;
        }
    }

private:
    std::unordered_map<std::string, Action> actions;
};