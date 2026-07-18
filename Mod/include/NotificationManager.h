#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class NotificationManager {
public:
    static void Initialize();
    [[nodiscard]] static bool Update();
    static void Render();

    static void NotifyStateChange(std::string_view actionName, bool enabled);
    static void NotifyAction(std::string_view actionName);

    [[nodiscard]] static bool IsEnabled() noexcept { return s_enabled.load(std::memory_order_acquire); }
    static void SetEnabled(bool enabled);

private:
    struct Notification {
        std::string message;
        float startTime;
        float duration;
        float textHeight = 0.0f;
        float renderWidth = 0.0f;
        float renderHeight = 0.0f;
        float layoutWidth = 0.0f;
    };

    static constexpr std::size_t MAX_NOTIFICATIONS = 5;
    static constexpr float MAX_NOTIFICATION_WIDTH = 420.0f;
    static constexpr float PADDING = 8.0f;
    static constexpr float FADE_IN_DURATION = 0.25f;
    static constexpr float FADE_OUT_DURATION = 0.4f;
    static constexpr float INV_FADE_IN = 1.0f / FADE_IN_DURATION;
    static constexpr float INV_FADE_OUT = 1.0f / FADE_OUT_DURATION;
    static constexpr float TEXT_PADDING_X = 16.0f;
    static constexpr float TEXT_PADDING_Y = 10.0f;

    static std::vector<Notification> s_notifications;
    static std::mutex s_mutex;
    static std::atomic_bool s_enabled;
    static std::atomic_bool s_hasNotifications;
    static float s_currentTime;

    [[nodiscard]] static float GetTime() noexcept;
    static void AddNotification(std::string_view text, std::string_view suffix, float duration);
    static void CacheLayout(Notification& notification, float maxWidth) noexcept;
    [[nodiscard]] static constexpr float CalculateAlpha(float elapsed, float duration) noexcept;
};
