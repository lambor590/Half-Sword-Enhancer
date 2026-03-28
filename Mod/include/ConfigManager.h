#pragma once

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
    bool suppressDeferred_ = false;
    static constexpr std::chrono::milliseconds SAVE_DELAY{500};

    ConfigManager();

public:
    static std::filesystem::path GetAppDataPath();

    static ConfigManager& Get() {
        static ConfigManager manager;
        return manager;
    }

    void SaveConfig();
    void SaveConfigDeferred();
    void LoadConfig();
    void FlushPendingSave();
    void SuppressDeferred(bool suppress) { suppressDeferred_ = suppress; }

    int GetInt(std::string_view function, std::string_view param, int defaultValue);

    bool GetBool(std::string_view function, std::string_view param, bool defaultValue);

    float GetFloat(std::string_view function, std::string_view param, float defaultValue);

    std::string GetString(std::string_view function, std::string_view param, std::string_view defaultValue);

    void SetInt(std::string_view function, std::string_view param, int value);

    void SetBool(std::string_view function, std::string_view param, bool value);

    void SetFloat(std::string_view function, std::string_view param, float value);

    void SetString(std::string_view function, std::string_view param, std::string_view value);
};
