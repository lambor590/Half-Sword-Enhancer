#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <filesystem>
#include <chrono>
#include <mutex>
#include <utility>

#include "../../ext/SimpleIni.h"

class ConfigManager {
private:
    CSimpleIni ini;
    std::filesystem::path configPath;
    std::string serializedConfig;
    mutable std::mutex mutex;
    std::chrono::steady_clock::time_point lastSaveAttempt{};
    bool needsSave = false;
    unsigned int batchDepth = 0;
    static constexpr std::chrono::milliseconds SAVE_DELAY{500};

    ConfigManager();
    [[nodiscard]] bool SaveConfigLocked() noexcept;
    void MarkChangedLocked();
    void BeginBatch();
    void EndBatch();

    class BatchGuard {
    public:
        explicit BatchGuard(ConfigManager& owner) : owner(owner) { owner.BeginBatch(); }
        ~BatchGuard() { owner.EndBatch(); }

        BatchGuard(const BatchGuard&) = delete;
        BatchGuard& operator=(const BatchGuard&) = delete;

    private:
        ConfigManager& owner;
    };

public:
    [[nodiscard]] static const std::filesystem::path& GetAppDataPath();
    static ConfigManager& Get();

    void SaveConfig();
    void FlushIfDue() noexcept;
    [[nodiscard]] bool Flush() noexcept;
    void LoadConfig();

    template <typename Updates> void BatchSave(Updates&& updates) {
        const BatchGuard batch(*this);
        std::invoke(std::forward<Updates>(updates));
    }

    int GetInt(const char* section, const char* key, int defaultValue);
    bool GetBool(const char* section, const char* key, bool defaultValue);
    float GetFloat(const char* section, const char* key, float defaultValue);
    double GetDouble(const char* section, const char* key, double defaultValue);
    std::string GetString(const char* section, const char* key, std::string_view defaultValue);

    void SetInt(const char* section, const char* key, int value);
    void SetBool(const char* section, const char* key, bool value);
    void SetFloat(const char* section, const char* key, float value);
    void SetDouble(const char* section, const char* key, double value);
    void SetString(const char* section, const char* key, const char* value);
    void DeleteSection(const char* section);
};
