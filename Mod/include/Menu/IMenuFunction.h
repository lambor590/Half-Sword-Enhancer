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
#include <mutex>
#include <array>

#include "imgui/imgui.h"
#include "GlobalDefinitions.h"
#include "Hooks/GameHook.h"
#include "DefaultStyle.h"

class TooltipHelper {
public:
    static void ShowTooltip(const char* tooltip);
    static void ShowTooltip(const std::string& tooltip);
    static void ShowTooltip(std::string_view tooltip);
    static void InvalidateCache();
};

class Parameter {
public:
    enum class Type : uint8_t { Int, Float, Bool };

    std::string_view name, displayName;
    std::string_view tooltip;
    Type type;
    void* valuePtr;
    union { int intMin, intMax; float floatMin, floatMax; } minValue{}, maxValue{};
    mutable std::string id;

private:
    using RenderFn = void(*)(const Parameter&) noexcept;
    using LoadFn = void(*)(const Parameter&, const class IMenuFunction*) noexcept;
    using SaveFn = void(*)(const Parameter&, const class IMenuFunction*) noexcept;
    
    RenderFn renderFn;
    LoadFn loadFn;
    SaveFn saveFn;
    
    template<typename T>
    static void RenderParameter(const Parameter& param) noexcept;
    
    template<typename T>
    static void LoadParameter(const Parameter& param, const IMenuFunction* func) noexcept;
    
    template<typename T>
    static void SaveParameter(const Parameter& param, const IMenuFunction* func) noexcept;
    
    static void RenderInt(const Parameter& param) noexcept;
    static void RenderFloat(const Parameter& param) noexcept;
    static void RenderBool(const Parameter& param) noexcept;
    
    static void LoadInt(const Parameter& param, const IMenuFunction* func) noexcept;
    static void LoadFloat(const Parameter& param, const IMenuFunction* func) noexcept;
    static void LoadBool(const Parameter& param, const IMenuFunction* func) noexcept;
    
    static void SaveInt(const Parameter& param, const IMenuFunction* func) noexcept;
    static void SaveFloat(const Parameter& param, const IMenuFunction* func) noexcept;
    static void SaveBool(const Parameter& param, const IMenuFunction* func) noexcept;

public:
    Parameter(std::string_view name, std::string_view displayName, int* valuePtr, int minValue = 0, int maxValue = 100, std::string_view tooltip = "") noexcept
        : name(name), displayName(displayName), tooltip(tooltip), type(Type::Int), valuePtr(valuePtr), 
          renderFn(RenderInt), loadFn(LoadInt), saveFn(SaveInt) {
        this->minValue.intMin = minValue;
        this->maxValue.intMax = maxValue;
        id = "##param_" + std::string(name);
    }

    Parameter(std::string_view name, std::string_view displayName, float* valuePtr, float minValue = 0.0f, float maxValue = 1.0f, std::string_view tooltip = "") noexcept
        : name(name), displayName(displayName), tooltip(tooltip), type(Type::Float), valuePtr(valuePtr), 
          renderFn(RenderFloat), loadFn(LoadFloat), saveFn(SaveFloat) {
        this->minValue.floatMin = minValue;
        this->maxValue.floatMax = maxValue;
        id = "##param_" + std::string(name);
    }

    Parameter(std::string_view name, std::string_view displayName, bool* valuePtr, std::string_view tooltip = "") noexcept
        : name(name), displayName(displayName), tooltip(tooltip), type(Type::Bool), valuePtr(valuePtr), 
          renderFn(RenderBool), loadFn(LoadBool), saveFn(SaveBool) {
        id = "##param_" + std::string(name);
    }

    void Render() const noexcept { renderFn(*this); }
    void Load(const class IMenuFunction* func) const noexcept { loadFn(*this, func); }
    void Save(const class IMenuFunction* func) const noexcept { saveFn(*this, func); }
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
    std::string tooltip;
    int* key;
    std::function<void(bool)> callback;
    bool waitingForKey = false;
    int prevKey = 0;
    bool toggleable = false;
    bool popupWasOpen = false;
    int pendingConflictKey = 0;
    int* pendingConflictKeyPtr = nullptr;
    int pendingOriginalKey = 0;
    mutable int cachedBindingCount = 0;
    mutable std::vector<IMenuFunction*> cachedBoundFunctions;

private:
    mutable std::once_flag m_idsInitialized;
    mutable std::array<std::string, 5> m_cachedIds;
    
    enum class IdIndex : size_t { Key = 0, Check = 1, Popup = 2, Conflict = 3, ParamButton = 4 };

    void InitializeIds() const {
        std::call_once(m_idsInitialized, [this]() {
            constexpr std::string_view prefix = 
                std::is_same_v<Derived, HookedFunction> ? "##Hook_" : "##Key_";
            
            std::string base;
            base.reserve(prefix.length() + name.length() + 10);
            base.append(prefix);
            base.append(name);
            
            m_cachedIds[static_cast<size_t>(IdIndex::Key)] = base + "_key";
            m_cachedIds[static_cast<size_t>(IdIndex::Check)] = base;
            m_cachedIds[static_cast<size_t>(IdIndex::Popup)] = base + "_params";
            m_cachedIds[static_cast<size_t>(IdIndex::Conflict)] = base + "_conflict";
            m_cachedIds[static_cast<size_t>(IdIndex::ParamButton)] = "Config##" + base;
        });
    }

protected:
    KeyFunction(std::string_view funcName, int* keyPtr, std::function<void(bool)> callback, bool toggleable, std::string_view funcTooltip = "")
        : name(funcName), tooltip(funcTooltip), key(keyPtr), callback(std::move(callback)), prevKey(*key), toggleable(toggleable) {}

    virtual void OnKeyAssigned() = 0;

public:
    void Render() override;
    std::string_view GetName() const override { return name; }
    std::string_view GetTooltip() const { return tooltip; }
    int GetKey() const { return key ? *key : 0; }
    const std::function<void(bool)>& GetCallback() const { return callback; }
    void ResetPrevKey() { prevKey = *key; }

    const char* GetKeyId() const { 
        InitializeIds(); 
        return m_cachedIds[static_cast<size_t>(IdIndex::Key)].c_str(); 
    }
    const char* GetCheckId() const { 
        InitializeIds(); 
        return m_cachedIds[static_cast<size_t>(IdIndex::Check)].c_str(); 
    }
    const char* GetPopupId() const { 
        InitializeIds(); 
        return m_cachedIds[static_cast<size_t>(IdIndex::Popup)].c_str(); 
    }
    const char* GetParamButtonId() const { 
        InitializeIds(); 
        return m_cachedIds[static_cast<size_t>(IdIndex::ParamButton)].c_str(); 
    }
    const char* GetConflictPopupId() const { 
        InitializeIds(); 
        return m_cachedIds[static_cast<size_t>(IdIndex::Conflict)].c_str(); 
    }

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
                   std::function<void(bool)> callback, int* keyPtr, bool executeOnToggle = false, std::string_view funcTooltip = "")
        : KeyFunction(funcName, keyPtr, std::move(callback), true, funcTooltip),
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
    KeybindFunction(std::string_view funcName, int* keyPtr, std::function<void(bool)> callback, bool toggleable = false, std::string_view funcTooltip = "")
        : KeyFunction(funcName, keyPtr, std::move(callback), toggleable, funcTooltip) {
        LoadConfig();
    }

    void LoadConfig();
    void SetEnabled(bool enabled) override;
    void UpdateKey();
};