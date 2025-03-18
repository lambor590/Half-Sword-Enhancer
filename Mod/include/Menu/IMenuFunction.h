#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <variant>

#include "imgui/imgui.h"
#include "GlobalDefinitions.h"

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
    std::function<void()> callback;
    int* key;
    bool waitingForKey = false;
    int prevKey = 0;

public:
    HookedFunction(const std::string& name, const std::string& hookedFunction, std::function<void()> callback, int* keyPtr)
        : name(name), hookedFunction(hookedFunction), callback(callback), key(keyPtr) {
        prevKey = *key;
        LoadConfig();
    }

    ~HookedFunction() override;

    void LoadConfig();
    void Render() override;
    const std::string& GetName() const override { return name; }
    const std::string& GetHookedFunction() const { return hookedFunction; }
    const std::function<void()>& GetCallback() const { return callback; }

    int GetKey() const { return key ? *key : 0; }
    void SetKey();
    bool IsEnabled() const { return isEnabled; }
    void SetEnabled(bool enabled) override;
};

class KeybindFunction : public IMenuFunction {
private:
    std::string name;
    int* key;
    std::function<void()> callback;
    bool waitingForKey = false;
    int prevKey = 0;

public:
    KeybindFunction(const std::string& name, int* key, std::function<void()> callback)
        : name(name), key(key), callback(callback), prevKey(*key) {
        LoadConfig();
    }

    void LoadConfig();
    void Render() override;
    const std::string& GetName() const override { return name; }
    int* GetKey() const { return key; }
    const std::function<void()>& GetCallback() const { return callback; }

    void UpdateKey();
};