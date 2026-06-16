#pragma once

#include <string>
#include <string_view>
#include <vector>

struct Notification {
    std::string message;
    float startTime;
    float duration;
    float textHeight = 0.0f;
    float renderWidth = 0.0f;

    Notification(std::string&& msg, float dur = 2.5f) noexcept;
};

class NotificationManager {
private:
    static std::vector<Notification> s_notifications;
    static bool s_enabled;
    static float s_currentTime;

    static constexpr size_t MAX_NOTIFICATIONS = 5;
    static constexpr float MIN_NOTIFICATION_WIDTH = 80.0f;
    static constexpr float NOTIFICATION_HEIGHT = 40.0f;
    static constexpr float PADDING = 8.0f;
    static constexpr float FADE_IN_DURATION = 0.25f;
    static constexpr float FADE_OUT_DURATION = 0.4f;
    static constexpr float INV_FADE_IN = 1.0f / FADE_IN_DURATION;
    static constexpr float INV_FADE_OUT = 1.0f / FADE_OUT_DURATION;
    static constexpr float TEXT_PADDING = 16.0f;
    static constexpr float HEIGHT_PLUS_PADDING = NOTIFICATION_HEIGHT + PADDING;

    NotificationManager() = default;

public:
    static void Initialize() noexcept;
    [[nodiscard]] static bool Update() noexcept;
    static void Render() noexcept;

    static void NotifyHookToggle(std::string_view functionName, bool enabled) noexcept;
    static void NotifyOneTimeAction(std::string_view actionName) noexcept;

    static bool IsEnabled() noexcept { return s_enabled; }
    static void SetEnabled(bool enabled) noexcept;
    static bool HasNotifications() noexcept { return !s_notifications.empty(); }

    static float GetTime() noexcept;

private:
    static void AddNotification(std::string&& message, float duration = 2.5f) noexcept;
    static void CacheLayout(Notification& notification) noexcept;
    [[nodiscard]] static constexpr float CalculateAlpha(float elapsed, float duration) noexcept;
};
