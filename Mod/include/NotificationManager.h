#pragma once

#include <string>
#include <chrono>
#include <deque>

#include "imgui/imgui.h"

class ConfigManager;
extern ConfigManager& g_ConfigManager;

struct Notification {
    std::string message;
    std::chrono::steady_clock::time_point startTime;
    float duration;
    static constexpr float fadeInDuration = 0.3f;
    static constexpr float fadeOutDuration = 0.5f;
    
    Notification(std::string&& msg, float dur = 2.5f) noexcept
        : message(std::move(msg)), startTime(std::chrono::steady_clock::now()), duration(dur) {}
};

class NotificationManager {
private:
    static std::deque<Notification> s_notifications;
    static bool s_enabled;
    static thread_local std::string s_stringBuffer;
    static constexpr size_t MAX_NOTIFICATIONS = 5;
    static constexpr float NOTIFICATION_WIDTH = 300.0f;
    static constexpr float NOTIFICATION_HEIGHT = 50.0f;
    static constexpr float PADDING = 10.0f;
    static constexpr ImVec4 NOTIFICATION_BG = ImVec4(0.1f, 0.1f, 0.1f, 0.9f);
    static constexpr ImVec4 NOTIFICATION_TEXT = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    
    NotificationManager() = default;

public:
    static void Initialize() noexcept;
    static void Update() noexcept;
    static void Render() noexcept;
    
    static void NotifyHookToggle(const std::string& functionName, bool enabled) noexcept;
    static void NotifyOneTimeAction(const std::string& actionName) noexcept;
    
    static bool IsEnabled() noexcept { return s_enabled; }
    static void SetEnabled(bool enabled) noexcept;
    
private:
    static void AddNotification(std::string&& message, float duration = 2.5f) noexcept;
    static float CalculateAlpha(const Notification& notification, float elapsed) noexcept;
};