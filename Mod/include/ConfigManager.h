#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <filesystem>
#include <chrono>
#include <atomic>

#include "../../ext/SimpleIni.h"

class ConfigManager {
private:
    CSimpleIni ini;
    std::filesystem::path configPath;
    mutable std::atomic<bool> needsSave{false};
    mutable std::chrono::steady_clock::time_point lastSaveTime;
    bool suppressDeferred = false;
    static constexpr std::chrono::milliseconds SAVE_DELAY{500};

    ConfigManager();

public:
    static std::filesystem::path GetAppDataPath();
    static ConfigManager& Get();

    void SaveConfig();
    void SaveConfigDeferred();
    void LoadConfig();
    void SuppressDeferred(bool suppress);

    /// Executes multiple Set* calls in a single batch, issuing one SaveConfig at the end.
    /// Use instead of calling SetX() individually when writing many values at once.
    void BatchSave(const std::function<void()>& updates);

    int GetInt(std::string_view section, std::string_view key, int defaultValue);
    bool GetBool(std::string_view section, std::string_view key, bool defaultValue);
    float GetFloat(std::string_view section, std::string_view key, float defaultValue);
    std::string GetString(std::string_view section, std::string_view key, std::string_view defaultValue);

    void SetInt(std::string_view section, std::string_view key, int value);
    void SetBool(std::string_view section, std::string_view key, bool value);
    void SetFloat(std::string_view section, std::string_view key, float value);
    void SetString(std::string_view section, std::string_view key, std::string_view value);
    void DeleteSection(std::string_view section);
};
