#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <memory>

class ConfigManager;
extern ConfigManager& g_ConfigManager;

struct Notification {
    std::string message;
    std::chrono::steady_clock::time_point startTime;
    float duration;
    float fadeInDuration;
    float fadeOutDuration;
    
    Notification(const std::string& msg, float dur = 3.0f, float fadeIn = 0.3f, float fadeOut = 0.5f)
        : message(msg), startTime(std::chrono::steady_clock::now()), 
          duration(dur), fadeInDuration(fadeIn), fadeOutDuration(fadeOut) {}
};

class NotificationManager {
private:
    static std::vector<std::unique_ptr<Notification>> s_notifications;
    static bool s_enabled;
    static bool s_initialized;
    
    NotificationManager() = default;

public:
    static void Initialize() noexcept;
    static void Update() noexcept;
    static void Render() noexcept;
    
    static void AddNotification(const std::string& message, float duration = 3.0f) noexcept;
    
    static void NotifyHookToggle(const std::string& functionName, bool enabled) noexcept;
    static void NotifyOneTimeAction(const std::string& actionName) noexcept;
    
    static bool IsEnabled() noexcept { return s_enabled; }
    static void SetEnabled(bool enabled) noexcept;
    
private:
    static float CalculateAlpha(const Notification& notification) noexcept;
};