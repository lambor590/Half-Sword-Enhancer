#pragma once

#include <Windows.h>
#include <string>
#include <string_view>
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
    enum class Type : uint8_t { Int, Float, Bool };

private:
    std::string_view name;
    std::string_view displayName;
    Type type;
    void* valuePtr;
    union { int intMin, intMax; float floatMin, floatMax; } minValue, maxValue;
    mutable std::string id;
    
    using RenderFn = void(*)(const Parameter&);
    using LoadFn = void(*)(const Parameter&, const class IMenuFunction*);
    using SaveFn = void(*)(const Parameter&, const class IMenuFunction*);
    
    RenderFn renderFn;
    LoadFn loadFn;
    SaveFn saveFn;
    
    static void RenderInt(const Parameter& param);
    static void RenderFloat(const Parameter& param);
    static void RenderBool(const Parameter& param);
    
    static void LoadInt(const Parameter& param, const class IMenuFunction* func);
    static void LoadFloat(const Parameter& param, const class IMenuFunction* func);
    static void LoadBool(const Parameter& param, const class IMenuFunction* func);
    
    static void SaveInt(const Parameter& param, const class IMenuFunction* func);
    static void SaveFloat(const Parameter& param, const class IMenuFunction* func);
    static void SaveBool(const Parameter& param, const class IMenuFunction* func);

public:
    Parameter(std::string_view name, std::string_view displayName, int* valuePtr, int minValue = 0, int maxValue = 100)
        : name(name), displayName(displayName), type(Type::Int), valuePtr(valuePtr), renderFn(RenderInt), loadFn(LoadInt), saveFn(SaveInt) {
        this->minValue.intMin = minValue;
        this->maxValue.intMax = maxValue;
        id.reserve(name.size() + 8);
        id = "##param_";
        id += name;
    }

    Parameter(std::string_view name, std::string_view displayName, float* valuePtr, float minValue = 0.0f, float maxValue = 1.0f)
        : name(name), displayName(displayName), type(Type::Float), valuePtr(valuePtr), renderFn(RenderFloat), loadFn(LoadFloat), saveFn(SaveFloat) {
        this->minValue.floatMin = minValue;
        this->maxValue.floatMax = maxValue;
        id.reserve(name.size() + 8);
        id = "##param_";
        id += name;
    }

    Parameter(std::string_view name, std::string_view displayName, bool* valuePtr)
        : name(name), displayName(displayName), type(Type::Bool), valuePtr(valuePtr), renderFn(RenderBool), loadFn(LoadBool), saveFn(SaveBool) {
        this->minValue.intMin = 0;
        this->maxValue.intMax = 1;
        id.reserve(name.size() + 8);
        id = "##param_";
        id += name;
    }

    void Render() const { renderFn(*this); }
    void Load(const class IMenuFunction* func) const { loadFn(*this, func); }
    void Save(const class IMenuFunction* func) const { saveFn(*this, func); }

    std::string_view GetName() const { return name; }
    std::string_view GetDisplayName() const { return displayName; }
    Type GetType() const { return type; }
    const std::string& GetId() const { return id; }

    int* GetIntPtr() const { return type == Type::Int ? static_cast<int*>(valuePtr) : nullptr; }
    float* GetFloatPtr() const { return type == Type::Float ? static_cast<float*>(valuePtr) : nullptr; }
    bool* GetBoolPtr() const { return type == Type::Bool ? static_cast<bool*>(valuePtr) : nullptr; }
    
    int GetIntMin() const { return minValue.intMin; }
    int GetIntMax() const { return maxValue.intMax; }
    float GetFloatMin() const { return minValue.floatMin; }
    float GetFloatMax() const { return maxValue.floatMax; }
};

class IMenuFunction {
protected:
    bool isEnabled = false;
    std::vector<Parameter> parameters;

public:
    virtual ~IMenuFunction() = default;
    virtual void Render() = 0;
    virtual std::string_view GetName() const = 0;
    virtual void OnKeyUnbound() {}

    static std::string NormalizeSection(std::string_view name) {
        std::string s(name);
        s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
        return s;
    }
    
    template<typename T>
    T GetConfig(std::string_view paramName, T defaultValue) const {
        auto section = NormalizeSection(GetName());
        if constexpr (std::is_same_v<T, int>)
            return g_ConfigManager.GetInt(section, std::string(paramName), defaultValue);
        else if constexpr (std::is_same_v<T, bool>)
            return g_ConfigManager.GetBool(section, std::string(paramName), defaultValue);
        else if constexpr (std::is_same_v<T, float>)
            return g_ConfigManager.GetFloat(section, std::string(paramName), defaultValue);
        else
            return g_ConfigManager.GetString(section, std::string(paramName), defaultValue);
    }
    
    template<typename T>
    void SaveConfig(std::string_view paramName, T value) const {
        auto section = NormalizeSection(GetName());
        if constexpr (std::is_same_v<T, int>)
            g_ConfigManager.SetInt(section, std::string(paramName), value);
        else if constexpr (std::is_same_v<T, bool>)
            g_ConfigManager.SetBool(section, std::string(paramName), value);
        else if constexpr (std::is_same_v<T, float>)
            g_ConfigManager.SetFloat(section, std::string(paramName), value);
        else
            g_ConfigManager.SetString(section, std::string(paramName), value);
    }

    virtual void SetEnabled(bool enabled) {
        if (isEnabled != enabled) {
            isEnabled = enabled;
            SaveConfig("enabled", enabled);
            g_ConfigManager.SaveConfig();
        }
    }
    
    bool LoadEnabledState(bool defaultState = false) {
        return isEnabled = GetConfig("enabled", defaultState);
    }

    void AddParameter(Parameter&& param) { parameters.emplace_back(std::move(param)); }
    void AddParameter(const Parameter& param) { parameters.emplace_back(param); }
    
    void RenderParameters() {
        for (auto& param : parameters)
            param.Render();
    }
    
    void LoadParameters() {
        for (const auto& param : parameters) {
            param.Load(this);
        }
    }
    
    void SaveParameters() const {
        for (const auto& param : parameters) {
            param.Save(this);
        }
        g_ConfigManager.SaveConfig();
    }
    
    const std::vector<Parameter>& GetParameters() const { return parameters; }
};

class HookedFunction;
class KeybindFunction;

template<typename Derived>
class KeyFunction : public IMenuFunction {
protected:
    std::string name;
    int* key;
    std::function<void(bool)> callback;
    bool waitingForKey = false;
    int prevKey = 0;
    bool toggleable = false;
    bool popupWasOpen = false;
    int pendingConflictKey = 0;
    int* pendingConflictKeyPtr = nullptr;

private:
    mutable bool idsInitialized = false;
    mutable std::string keyId, checkId, popupId, conflictPopupId, paramButtonId;

    void InitializeIds() const {
        if (!idsInitialized) {
            std::string_view prefix = std::is_same_v<Derived, HookedFunction> ? "##Hook_" : "##Key_";
            std::string base = std::string(prefix) + name;
            
            keyId = base + "_key";
            checkId = base;
            popupId = base + "_params";
            conflictPopupId = base + "_conflict";
            paramButtonId = "Config##" + base;
            
            idsInitialized = true;
        }
    }

protected:
    KeyFunction(std::string_view funcName, int* keyPtr, std::function<void(bool)> callback, bool toggleable)
        : name(funcName), key(keyPtr), callback(std::move(callback)), prevKey(*key), toggleable(toggleable) {}

    virtual void OnKeyAssigned() = 0;

public:
    void Render() override;
    std::string_view GetName() const override { return name; }
    int GetKey() const { return key ? *key : 0; }
    const std::function<void(bool)>& GetCallback() const { return callback; }
    void ResetPrevKey() { prevKey = *key; }

    const char* GetKeyId() const { InitializeIds(); return keyId.c_str(); }
    const char* GetCheckId() const { InitializeIds(); return checkId.c_str(); }
    const char* GetPopupId() const { InitializeIds(); return popupId.c_str(); }
    const char* GetParamButtonId() const { InitializeIds(); return paramButtonId.c_str(); }
    const char* GetConflictPopupId() const { InitializeIds(); return conflictPopupId.c_str(); }

    void OnKeyUnbound() override {
        ResetPrevKey();
        SaveConfig("key", -1);
    }
};

class HookedFunction : public KeyFunction<HookedFunction> {
private:
    std::vector<GameHook::GameEvent> eventTypes;
    bool executeOnToggle = false;

protected:
    void OnKeyAssigned() override;

public:
    HookedFunction(std::string_view funcName, const std::vector<GameHook::GameEvent>& events,
                   std::function<void(bool)> callback, int* keyPtr, bool executeOnToggle = false)
        : KeyFunction(funcName, keyPtr, std::move(callback), true),
          eventTypes(events), executeOnToggle(executeOnToggle) {
        LoadConfig();
    }

    ~HookedFunction() override;
    void LoadConfig();
    void SetEnabled(bool enabled) override;
    void SetKey();
};

class KeybindFunction : public KeyFunction<KeybindFunction> {
protected:
    void OnKeyAssigned() override;

public:
    KeybindFunction(std::string_view funcName, int* keyPtr, std::function<void(bool)> callback, bool toggleable = false)
        : KeyFunction(funcName, keyPtr, std::move(callback), toggleable) {
        LoadConfig();
    }

    void LoadConfig();
    void SetEnabled(bool enabled) override;
    void UpdateKey();
};