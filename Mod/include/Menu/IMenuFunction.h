#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <variant>
#include <algorithm>
#include <type_traits>
#include <utility>

#include "imgui/imgui.h"
#include "GlobalDefinitions.h"
#include "Hooks/GameHook.h"

class Parameter {
public:
    enum class Type {
        Int,
        Float,
        Bool
    };

private:
    std::string id;
    std::string name;
    std::string displayName;
    Type type;
    std::variant<int*, float*, bool*> valuePtr;
    std::variant<int, float, bool> minValue;
    std::variant<int, float, bool> maxValue;

public:
    Parameter(const std::string& name, const std::string& displayName, int* valuePtr,
        int minValue = 0, int maxValue = 100)
        : name(name), displayName(displayName), type(Type::Int),
        valuePtr(valuePtr), minValue(minValue), maxValue(maxValue) {
        id = "##param_" + name;
    }

    Parameter(const std::string& name, const std::string& displayName, float* valuePtr,
        float minValue = 0.0f, float maxValue = 1.0f)
        : name(name), displayName(displayName), type(Type::Float),
        valuePtr(valuePtr), minValue(minValue), maxValue(maxValue) {
        id = "##param_" + name;
    }

    Parameter(const std::string& name, const std::string& displayName, bool* valuePtr)
        : name(name), displayName(displayName), type(Type::Bool),
        valuePtr(valuePtr), minValue(false), maxValue(true) {
        id = "##param_" + name;
    }

    void Render();

    const std::string& GetName() const { return name; }
    const std::string& GetDisplayName() const { return displayName; }
    Type GetType() const { return type; }

    int* GetIntPtr() const { return type == Type::Int ? std::get<int*>(valuePtr) : nullptr; }
    float* GetFloatPtr() const { return type == Type::Float ? std::get<float*>(valuePtr) : nullptr; }
    bool* GetBoolPtr() const { return type == Type::Bool ? std::get<bool*>(valuePtr) : nullptr; }
};

class IMenuFunction {
protected:
    bool isEnabled = false;
    std::vector<Parameter> parameters;

public:
    virtual ~IMenuFunction() = default;
    virtual void Render() = 0;
    virtual const std::string& GetName() const = 0;

    static std::string NormalizeSection(const std::string& name) {
        std::string s = name;
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
        return s;
    }
    
    template<typename T>
    T GetConfig(const std::string& paramName, T defaultValue) const {
        auto section = NormalizeSection(GetName());
        if constexpr (std::is_same_v<T, int>)
            return g_ConfigManager.GetInt(section, paramName, defaultValue);
        else if constexpr (std::is_same_v<T, bool>)
            return g_ConfigManager.GetBool(section, paramName, defaultValue);
        else if constexpr (std::is_same_v<T, float>)
            return g_ConfigManager.GetFloat(section, paramName, defaultValue);
        else
            return g_ConfigManager.GetString(section, paramName, defaultValue);
    }
    
    template<typename T>
    void SaveConfig(const std::string& paramName, T value) const {
        static_assert(
            std::is_same_v<T, int> || std::is_same_v<T, bool> ||
            std::is_same_v<T, float> || std::is_same_v<T, std::string>,
            "Unsupported config type"
        );
        auto section = NormalizeSection(GetName());
        if constexpr (std::is_same_v<T, int>)
            g_ConfigManager.SetInt(section, paramName, value);
        else if constexpr (std::is_same_v<T, bool>)
            g_ConfigManager.SetBool(section, paramName, value);
        else if constexpr (std::is_same_v<T, float>)
            g_ConfigManager.SetFloat(section, paramName, value);
        else
            g_ConfigManager.SetString(section, paramName, value);
    }

    virtual void SetEnabled(bool enabled);
    bool LoadEnabledState(bool defaultState = false);

    void AddParameter(const Parameter& param) { parameters.push_back(param); }
    void RenderParameters();
    void LoadParameters();
    void SaveParameters() const;
    const std::vector<Parameter>& GetParameters() const { return parameters; }
};

class HookedFunction : public IMenuFunction {
private:
    std::string name;
    std::string hookedFunction;
    bool useEvent = false;
    GameHook::GameEvent eventType;
    std::function<void(bool)> callback;
    int* key;
    bool waitingForKey = false;
    int prevKey = 0;
    bool executeOnToggle = false;

public:
    HookedFunction(const std::string& name, const std::string& hookedFunction, std::function<void(bool)> callback, int* keyPtr, bool executeOnToggle = false)
        : name(name), hookedFunction(hookedFunction), callback(std::move(callback)), key(keyPtr), executeOnToggle(executeOnToggle) {
        prevKey = *key;
        LoadConfig();
    }

    HookedFunction(const std::string& name, GameHook::GameEvent event, std::function<void(bool)> callback, int* keyPtr, bool executeOnToggle = false)
        : name(name), callback(std::move(callback)), key(keyPtr), useEvent(true), eventType(event), executeOnToggle(executeOnToggle) {
        prevKey = *key;
        LoadConfig();
    }

    ~HookedFunction() override;

    void LoadConfig();
    void Render() override;
    const std::string& GetName() const override { return name; }
    const std::string& GetHookedFunction() const { return hookedFunction; }
    const std::function<void(bool)>& GetCallback() const { return callback; }

    int GetKey() const { return key ? *key : 0; }
    void SetKey();
    bool IsEnabled() const { return isEnabled; }
    void SetEnabled(bool enabled) override;
};

class KeybindFunction : public IMenuFunction {
private:
    std::string name;
    int* key;
    std::function<void(bool)> callback;
    bool waitingForKey = false;
    int prevKey = 0;
    bool toggleable = false;

public:
    KeybindFunction(const std::string& name, int* key, std::function<void(bool)> callback, bool toggleable = false)
        : name(name), key(key), callback(std::move(callback)), toggleable(toggleable), prevKey(*key) {
        LoadConfig();
    }

    void LoadConfig();
    void Render() override;
    void SetEnabled(bool enabled) override;
    bool IsEnabled() const { return isEnabled; }
    const std::string& GetName() const override { return name; }
    int* GetKey() const { return key; }
    const std::function<void(bool)>& GetCallback() const { return callback; }

    void UpdateKey();
};