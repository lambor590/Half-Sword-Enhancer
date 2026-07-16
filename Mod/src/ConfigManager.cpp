#include <Windows.h>
#include <ShlObj.h>
#include <KnownFolders.h>

#include <fstream>
#include <string>

#include "ConfigManager.h"

const std::filesystem::path& ConfigManager::GetAppDataPath() {
    static const std::filesystem::path CACHED = [] {
        PWSTR appDataPath = nullptr;
        std::filesystem::path result;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
            result = std::filesystem::path(appDataPath) / "Half Sword Enhancer";
            CoTaskMemFree(appDataPath);
            std::filesystem::create_directories(result);
        } else {
            result = std::filesystem::current_path();
        }
        return result;
    }();
    return CACHED;
}

ConfigManager& ConfigManager::Get() {
    static ConfigManager manager;
    return manager;
}

ConfigManager::ConfigManager() {
    configPath = GetAppDataPath() / "config.ini";

    ini.SetUnicode();
    LoadConfig();
}

bool ConfigManager::SaveConfigLocked() noexcept {
    lastSaveAttempt = std::chrono::steady_clock::now();
    try {
        std::error_code error;
        std::filesystem::create_directories(configPath.parent_path(), error);
        if (error) return false;

        serializedConfig.clear();
        if (ini.Save(serializedConfig) < 0) return false;

        auto temporary = configPath;
        temporary += L".tmp";

        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            output.write(serializedConfig.data(), static_cast<std::streamsize>(serializedConfig.size()));
            output.close();
            if (!output) {
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

        if (!MoveFileExW(temporary.c_str(), configPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, error);
            return false;
        }

        needsSave = false;
        return true;
    } catch (...) {
        return false;
    }
}

void ConfigManager::MarkChangedLocked() {
    needsSave = true;
    if (batchDepth == 0 && std::chrono::steady_clock::now() - lastSaveAttempt >= SAVE_DELAY) (void)SaveConfigLocked();
}

void ConfigManager::SaveConfig() {
    const std::lock_guard lock(mutex);
    if (needsSave) (void)SaveConfigLocked();
}

void ConfigManager::FlushIfDue() noexcept {
    try {
        const std::lock_guard lock(mutex);
        if (needsSave && batchDepth == 0 && std::chrono::steady_clock::now() - lastSaveAttempt >= SAVE_DELAY)
            (void)SaveConfigLocked();
    } catch (...) {
        return;
    }
}

bool ConfigManager::Flush() noexcept {
    try {
        const std::lock_guard lock(mutex);
        if (!needsSave) return true;
        if (batchDepth != 0) return false;
        return SaveConfigLocked();
    } catch (...) {
        return false;
    }
}

void ConfigManager::BeginBatch() {
    const std::lock_guard lock(mutex);
    ++batchDepth;
}

void ConfigManager::EndBatch() {
    const std::lock_guard lock(mutex);
    --batchDepth;
    if (batchDepth == 0 && needsSave) (void)SaveConfigLocked();
}

void ConfigManager::LoadConfig() {
    const std::lock_guard lock(mutex);
    std::error_code error;
    if (std::filesystem::is_regular_file(configPath, error) && !error) {
        if (ini.LoadFile(configPath.string().c_str()) >= 0) return;
        ini.Reset();
        ini.SetUnicode();
    }
    needsSave = true;
    (void)SaveConfigLocked();
}

int ConfigManager::GetInt(const char* section, const char* key, int defaultValue) {
    const std::lock_guard lock(mutex);
    return ini.GetLongValue(section, key, defaultValue);
}

bool ConfigManager::GetBool(const char* section, const char* key, bool defaultValue) {
    const std::lock_guard lock(mutex);
    return ini.GetBoolValue(section, key, defaultValue);
}

float ConfigManager::GetFloat(const char* section, const char* key, float defaultValue) {
    const std::lock_guard lock(mutex);
    return static_cast<float>(ini.GetDoubleValue(section, key, defaultValue));
}

double ConfigManager::GetDouble(const char* section, const char* key, double defaultValue) {
    const std::lock_guard lock(mutex);
    return ini.GetDoubleValue(section, key, defaultValue);
}

std::string ConfigManager::GetString(const char* section, const char* key, std::string_view defaultValue) {
    const std::lock_guard lock(mutex);
    const char* value = ini.GetValue(section, key, nullptr);
    return value ? std::string(value) : std::string(defaultValue);
}

void ConfigManager::SetInt(const char* section, const char* key, int value) {
    const std::lock_guard lock(mutex);
    ini.SetLongValue(section, key, value);
    MarkChangedLocked();
}

void ConfigManager::SetBool(const char* section, const char* key, bool value) {
    const std::lock_guard lock(mutex);
    ini.SetBoolValue(section, key, value);
    MarkChangedLocked();
}

void ConfigManager::SetFloat(const char* section, const char* key, float value) {
    const std::lock_guard lock(mutex);
    ini.SetDoubleValue(section, key, value);
    MarkChangedLocked();
}

void ConfigManager::SetDouble(const char* section, const char* key, double value) {
    const std::lock_guard lock(mutex);
    ini.SetDoubleValue(section, key, value);
    MarkChangedLocked();
}

void ConfigManager::SetString(const char* section, const char* key, const char* value) {
    const std::lock_guard lock(mutex);
    ini.SetValue(section, key, value);
    MarkChangedLocked();
}

void ConfigManager::DeleteSection(const char* section) {
    const std::lock_guard lock(mutex);
    ini.Delete(section, nullptr);
    MarkChangedLocked();
}
