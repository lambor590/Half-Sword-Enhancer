#pragma once

#include <string>
#include <chrono>
#include <vector>

#include "imgui/imgui.h"

class ConfigManager;
extern ConfigManager& g_ConfigManager;

struct Notification {
    std::string message;
    float startTime;
    float duration;
    
    Notification(std::string&& msg, float dur = 2.5f) noexcept;
};

class NotificationManager {
private:
    static std::vector<Notification> s_notifications;
    static bool s_enabled;
    static float s_currentTime;
    
    static constexpr size_t MAX_NOTIFICATIONS = 5;
    static constexpr float NOTIFICATION_WIDTH = 320.0f;
    static constexpr float NOTIFICATION_HEIGHT = 60.0f;
    static constexpr float PADDING = 8.0f;
    static constexpr float FADE_IN_DURATION = 0.25f;
    static constexpr float FADE_OUT_DURATION = 0.4f;
    static constexpr float INV_FADE_IN = 1.0f / FADE_IN_DURATION;
    static constexpr float INV_FADE_OUT = 1.0f / FADE_OUT_DURATION;
    static constexpr float CHECK_INTERVAL = 0.1f;
    static constexpr float SLIDE_DISTANCE = 20.0f;
    static constexpr float TEXT_PADDING = 16.0f;
    static constexpr float TEXT_WRAP_WIDTH = NOTIFICATION_WIDTH - (TEXT_PADDING * 2.0f);
    static constexpr float HEIGHT_PLUS_PADDING = NOTIFICATION_HEIGHT + PADDING;
    
    static constexpr ImVec4 NOTIFICATION_BG = ImVec4(0.12f, 0.09f, 0.06f, 0.95f);
    static constexpr ImVec4 NOTIFICATION_TEXT = ImVec4(0.95f, 0.92f, 0.85f, 1.0f);
    static constexpr ImVec4 NOTIFICATION_BORDER = ImVec4(0.71f, 0.57f, 0.25f, 1.0f);
    
    
    NotificationManager() = default;

public:
    static void Initialize() noexcept;
    static void Update() noexcept;
    static void Render() noexcept;
    
    static void NotifyHookToggle(const std::string& functionName, bool enabled) noexcept;
    static void NotifyOneTimeAction(const std::string& actionName) noexcept;
    
    static bool IsEnabled() noexcept { return s_enabled; }
    static void SetEnabled(bool enabled) noexcept;
    static bool HasNotifications() noexcept { return !s_notifications.empty(); }

    static float GetTime() noexcept;
    
private:
    static void AddNotification(std::string&& message, float duration = 2.5f) noexcept;
    [[nodiscard]] static constexpr float CalculateAlpha(float elapsed, float duration) noexcept;
};