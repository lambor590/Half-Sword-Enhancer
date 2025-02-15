#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <memory>

#include "imgui.h"
#include "GameHook.h"

extern GameHook* g_GameHook;

class IMenuFunction {
protected:
    bool isEnabled = false;

public:
    virtual ~IMenuFunction() = default;
    virtual void Render() = 0;
    virtual const std::string& GetName() const = 0;
    bool IsEnabled() const { return isEnabled; }
};

class HookedFunction : public IMenuFunction {
private:
    std::string name;
    std::string hookedFunction;
    std::function<void()> callback;

public:
    HookedFunction(const std::string& name, const std::string& hookedFunction, std::function<void()> callback)
        : name(name), hookedFunction(hookedFunction), callback(callback) {}

    ~HookedFunction() {
        if (isEnabled) {
            g_GameHook->UnregisterHook(hookedFunction);
        }
    }

    void Render() override;
    const std::string& GetName() const override { return name; }
    const std::string& GetHookedFunction() const { return hookedFunction; }
    const std::function<void()>& GetCallback() const { return callback; }
};

class KeybindFunction : public IMenuFunction {
private:
    std::string name;
    int* key;
    std::function<void()> callback;
    bool waitingForKey = false;

public:
    KeybindFunction(const std::string& name, int* key, std::function<void()> callback)
        : name(name), key(key), callback(callback) {}

    void Render() override;
    const std::string& GetName() const override { return name; }
    int* GetKey() const { return key; }
    const std::function<void()>& GetCallback() const { return callback; }
}; 