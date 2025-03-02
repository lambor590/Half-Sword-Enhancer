#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

#include "imgui/imgui.h"
#include "GlobalDefinitions.h"

class IMenuFunction {
protected:
    bool isEnabled = false;
    
public:
    virtual ~IMenuFunction() = default;
    virtual void Render() = 0;
    virtual const std::string& GetName() const = 0;
    bool IsEnabled() const { return isEnabled; }
    
    int GetConfig(const std::string& paramName, int defaultValue) const;
    bool GetConfig(const std::string& paramName, bool defaultValue) const;
    float GetConfig(const std::string& paramName, float defaultValue) const;
    std::string GetConfig(const std::string& paramName, const std::string& defaultValue) const;
    
    void SaveConfig(const std::string& paramName, int value) const;
    void SaveConfig(const std::string& paramName, bool value) const;
    void SaveConfig(const std::string& paramName, float value) const;
    void SaveConfig(const std::string& paramName, const std::string& value) const;
    
    virtual void SetEnabled(bool enabled);
    bool LoadEnabledState(bool defaultState = false);
};

class HookedFunction : public IMenuFunction {
private:
    std::string name;
    std::string hookedFunction;
    std::function<void()> callback;
    int* key;
    bool waitingForKey = false;
    int prevKey = 0;

public:
    HookedFunction(const std::string& name, const std::string& hookedFunction, std::function<void()> callback, int* keyPtr)
        : name(name), hookedFunction(hookedFunction), callback(callback), key(keyPtr) {
        prevKey = *key;
    }

    ~HookedFunction() override;
    
    void LoadConfig();
    void Render() override;
    const std::string& GetName() const override { return name; }
    const std::string& GetHookedFunction() const { return hookedFunction; }
    const std::function<void()>& GetCallback() const { return callback; }
    
    int GetKey() const { return key ? *key : 0; }
    void SetKey(int newKey);
    void SetEnabled(bool enabled) override;
};

class KeybindFunction : public IMenuFunction {
private:
    std::string name;
    int* key;
    std::function<void()> callback;
    bool waitingForKey = false;
    int prevKey = VK_ESCAPE;

public:
    KeybindFunction(const std::string& name, int* key, std::function<void()> callback)
        : name(name), key(key), callback(callback), prevKey(*key) {}
    
    void LoadConfig();
    void Render() override;
    const std::string& GetName() const override { return name; }
    int* GetKey() const { return key; }
    const std::function<void()>& GetCallback() const { return callback; }
    
    void UpdateKey(int newKey);
}; 