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

    inline virtual void SetEnabled(bool enabled) {
        if (isEnabled != enabled) {
            isEnabled = enabled;
            SaveConfig("enabled", enabled);
            g_ConfigManager.SaveConfig();
        }
    }
    inline bool LoadEnabledState(bool defaultState = false) {
        return isEnabled = GetConfig("enabled", defaultState);
    }

    void AddParameter(const Parameter& param) { parameters.push_back(param); }
    inline void RenderParameters() {
        for (auto& param : parameters)
            param.Render();
    }
    inline void LoadParameters() {
        for (auto& param : parameters) {
            switch (param.GetType()) {
                case Parameter::Type::Int:
                    *param.GetIntPtr() = GetConfig(param.GetName(), *param.GetIntPtr());
                    break;
                case Parameter::Type::Float:
                    *param.GetFloatPtr() = GetConfig(param.GetName(), *param.GetFloatPtr());
                    break;
                case Parameter::Type::Bool:
                    *param.GetBoolPtr() = GetConfig(param.GetName(), *param.GetBoolPtr());
                    break;
            }
        }
    }
    inline void SaveParameters() const {
        for (const auto& param : parameters) {
            switch (param.GetType()) {
                case Parameter::Type::Int:
                    SaveConfig(param.GetName(), *param.GetIntPtr());
                    break;
                case Parameter::Type::Float:
                    SaveConfig(param.GetName(), *param.GetFloatPtr());
                    break;
                case Parameter::Type::Bool:
                    SaveConfig(param.GetName(), *param.GetBoolPtr());
                    break;
            }
        }
        g_ConfigManager.SaveConfig();
    }
    const std::vector<Parameter>& GetParameters() const { return parameters; }
};

class HookedFunction : public IMenuFunction {
private:
    std::string name;
    // imgui ids
    std::string idPrefix;
    std::string keyId;
    std::string checkId;
    std::string paramButtonId;
    std::string popupId;
    bool popupWasOpen = false;

    std::vector<GameHook::GameEvent> eventTypes;
    std::function<void(bool)> callback;
    int* key;
    bool waitingForKey = false;
    int prevKey = 0;
    bool executeOnToggle = false;

public:
    HookedFunction(const std::string& funcName,
                   const std::vector<GameHook::GameEvent>& events,
                   std::function<void(bool)> callback, int* keyPtr, bool executeOnToggle = false)
        : name(funcName),
          idPrefix("##Hook_" + funcName),
          keyId(idPrefix + "_key"),
          checkId("##check" + idPrefix),
          paramButtonId("Config##param_" + idPrefix),
          popupId("ConfigParams" + idPrefix),
          callback(std::move(callback)),
          key(keyPtr),
          prevKey(*key),
          executeOnToggle(executeOnToggle),
          eventTypes(events) {
        LoadConfig();
    }

    ~HookedFunction() override;

    void LoadConfig();
    void Render() override;
    const std::string& GetName() const override { return name; }
    const std::function<void(bool)>& GetCallback() const { return callback; }

    int GetKey() const { return key ? *key : 0; }
    void SetKey();
    bool IsEnabled() const { return isEnabled; }
    void SetEnabled(bool enabled) override;
};

class KeybindFunction : public IMenuFunction {
private:
    std::string name;
    // imgui ids
    std::string idPrefix;
    std::string keyId;
    std::string paramButtonId;
    std::string popupId;
    bool popupWasOpen = false;

    int* key;
    std::function<void(bool)> callback;
    bool waitingForKey = false;
    int prevKey = 0;
    bool toggleable = false;

public:
    KeybindFunction(const std::string& funcName, int* keyPtr,
                    std::function<void(bool)> callback, bool toggleable = false)
        : name(funcName),
          idPrefix("##Key_" + funcName),
          keyId(idPrefix),
          paramButtonId("Config##param_" + idPrefix),
          popupId("ConfigParams" + idPrefix),
          key(keyPtr),
          callback(std::move(callback)),
          toggleable(toggleable),
          prevKey(*key) {
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