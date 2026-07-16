#include "NotificationManager.h"
#include "ConfigManager.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <chrono>
#include <string_view>

namespace {
    constexpr std::string_view ENABLED_SUFFIX = " - On";
    constexpr std::string_view DISABLED_SUFFIX = " - Off";
    constexpr ImVec4 NOTIFICATION_BG{0.12f, 0.09f, 0.06f, 0.95f};
    constexpr ImVec4 NOTIFICATION_TEXT{0.95f, 0.92f, 0.85f, 1.0f};
    constexpr ImVec4 NOTIFICATION_BORDER{0.71f, 0.57f, 0.25f, 1.0f};
}

std::vector<Notification> NotificationManager::s_notifications;
std::mutex NotificationManager::s_mutex;
std::atomic_bool NotificationManager::s_enabled = true;
float NotificationManager::s_currentTime = 0.0f;


Notification::Notification(std::string&& msg, float dur) noexcept
    : message(std::move(msg)), startTime(NotificationManager::GetTime()), duration(dur) {}

float NotificationManager::GetTime() noexcept {
    static auto startTime = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - startTime).count();
}

void NotificationManager::Initialize() {
    s_enabled.store(ConfigManager::Get().GetBool("Notifications", "enabled", true), std::memory_order_release);
    s_notifications.reserve(MAX_NOTIFICATIONS);
}

bool NotificationManager::Update() {
    if (!s_enabled.load(std::memory_order_acquire)) return false;
    const std::scoped_lock lock(s_mutex);
    if (s_notifications.empty()) [[likely]]
        return false;

    s_currentTime = GetTime();

    std::erase_if(s_notifications, [currentTime = s_currentTime](const Notification& notification) noexcept -> bool {
        return currentTime - notification.startTime >= notification.duration;
    });
    return !s_notifications.empty();
}

void NotificationManager::CacheLayout(Notification& notification, float maxWidth) noexcept {
    if (notification.renderWidth > 0.0f && notification.layoutWidth == maxWidth) return;

    const float wrapWidth = maxWidth - TEXT_PADDING_X * 2.0f;
    const ImVec2 textSize = ImGui::CalcTextSize(
        notification.message.c_str(), notification.message.c_str() + notification.message.size(), false, wrapWidth
    );
    notification.textHeight = textSize.y;
    notification.renderWidth = std::clamp(textSize.x + TEXT_PADDING_X * 2.0f, MIN_NOTIFICATION_WIDTH, maxWidth);
    notification.renderHeight = (std::max)(textSize.y + TEXT_PADDING_Y * 2.0f, MIN_NOTIFICATION_HEIGHT);
    notification.layoutWidth = maxWidth;
}

void NotificationManager::Render() {
    if (!s_enabled.load(std::memory_order_acquire)) [[unlikely]]
        return;
    const std::scoped_lock lock(s_mutex);
    if (s_notifications.empty()) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float maxNotificationWidth =
        std::clamp(viewport->WorkSize.x * 0.35f, MIN_NOTIFICATION_WIDTH, MAX_NOTIFICATION_WIDTH);
    size_t visibleCount = 0;
    float containerWidth = MIN_NOTIFICATION_WIDTH;
    float containerHeight = 0.0f;

    for (auto& notification : s_notifications) {
        const float elapsed = s_currentTime - notification.startTime;
        if (CalculateAlpha(elapsed, notification.duration) > 0.0f) {
            ++visibleCount;
            CacheLayout(notification, maxNotificationWidth);
            if (notification.renderWidth > containerWidth) containerWidth = notification.renderWidth;
            containerHeight += notification.renderHeight;
        }
    }

    if (visibleCount == 0) [[unlikely]]
        return;

    containerHeight += PADDING * static_cast<float>(visibleCount - 1);
    const ImVec2 containerPos{
        viewport->WorkPos.x + viewport->WorkSize.x - containerWidth - PADDING,
        viewport->WorkPos.y + PADDING,
    };

    static constexpr ImGuiWindowFlags CONTAINER_FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    static constexpr ImGuiWindowFlags CHILD_FLAGS =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(containerPos);
    ImGui::SetNextWindowSize(ImVec2(containerWidth, containerHeight));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, PADDING));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    if (ImGui::Begin("##notifications_container", nullptr, CONTAINER_FLAGS)) [[likely]] {
        for (int i = static_cast<int>(s_notifications.size()) - 1; i >= 0; --i) {
            auto& notification = s_notifications[i];
            const float elapsed = s_currentTime - notification.startTime;
            const float alpha = CalculateAlpha(elapsed, notification.duration);

            if (alpha <= 0.0f) [[unlikely]]
                continue;

            CacheLayout(notification, maxNotificationWidth);
            const float notifWidth = notification.renderWidth;

            auto withAlpha = [alpha](const ImVec4& c) {
                return ImVec4(c.x, c.y, c.z, c.w * alpha);
            };
            const ImVec4 bgColor = withAlpha(NOTIFICATION_BG);
            const ImVec4 borderColor = withAlpha(NOTIFICATION_BORDER);
            const ImVec4 textColor = withAlpha(NOTIFICATION_TEXT);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
            ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);

            ImGui::SetCursorPosX(containerWidth - notifWidth);

            ImGui::PushID(i);
            if (ImGui::BeginChild("##notification", ImVec2(notifWidth, notification.renderHeight), true, CHILD_FLAGS))
                [[likely]] {
                const float textY = (notification.renderHeight - notification.textHeight) * 0.5f;
                ImGui::SetCursorPos(ImVec2(TEXT_PADDING_X, textY));
                ImGui::PushTextWrapPos(notifWidth - TEXT_PADDING_X);
                ImGui::TextUnformatted(
                    notification.message.c_str(), notification.message.c_str() + notification.message.size()
                );
                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();
            ImGui::PopID();

            ImGui::PopStyleColor(3);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(6);
}

void NotificationManager::AddNotification(std::string_view text, std::string_view suffix, float duration) {
    if (!s_enabled.load(std::memory_order_acquire)) [[unlikely]]
        return;
    const std::scoped_lock lock(s_mutex);
    if (!s_enabled.load(std::memory_order_relaxed)) return;

    if (s_notifications.size() >= MAX_NOTIFICATIONS) [[unlikely]] {
        s_notifications.erase(s_notifications.begin());
    }

    std::string message;
    message.reserve(text.size() + suffix.size());
    message.append(text);
    message.append(suffix);
    s_notifications.emplace_back(std::move(message), duration);
}

void NotificationManager::NotifyHookToggle(std::string_view functionName, bool enabled) {
    AddNotification(functionName, enabled ? ENABLED_SUFFIX : DISABLED_SUFFIX);
}

void NotificationManager::NotifyOneTimeAction(std::string_view actionName) {
    static constexpr float ACTION_DURATION = 2.0f;

    AddNotification(actionName, {}, ACTION_DURATION);
}

[[nodiscard]] constexpr float NotificationManager::CalculateAlpha(float elapsed, float duration) noexcept {
    if (elapsed < FADE_IN_DURATION) [[unlikely]] {
        return elapsed * INV_FADE_IN;
    }

    const float fadeStartTime = duration - FADE_OUT_DURATION;
    if (elapsed > fadeStartTime) [[unlikely]] {
        return 1.0f - (elapsed - fadeStartTime) * INV_FADE_OUT;
    }

    return 1.0f;
}

void NotificationManager::SetEnabled(bool enabled) {
    if (s_enabled.exchange(enabled, std::memory_order_acq_rel) == enabled) return;

    ConfigManager::Get().SetBool("Notifications", "enabled", enabled);
    ConfigManager::Get().SaveConfig();

    if (!enabled) {
        const std::scoped_lock lock(s_mutex);
        s_notifications.clear();
    }
}

bool NotificationManager::HasNotifications() {
    const std::scoped_lock lock(s_mutex);
    return !s_notifications.empty();
}
