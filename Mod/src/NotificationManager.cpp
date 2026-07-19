#include "NotificationManager.h"

#include "ConfigManager.h"
#include "imgui/imgui.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace {
    constexpr std::string_view ENABLED_SUFFIX = " - On";
    constexpr std::string_view DISABLED_SUFFIX = " - Off";
    constexpr ImVec4 NOTIFICATION_BG{0.12f, 0.09f, 0.06f, 0.95f};
    constexpr ImVec4 NOTIFICATION_TEXT{0.95f, 0.92f, 0.85f, 1.0f};
    constexpr ImVec4 NOTIFICATION_BORDER{0.71f, 0.57f, 0.25f, 1.0f};
    constexpr float DEFAULT_DURATION = 2.5f;
    constexpr float ACTION_DURATION = 2.0f;
}

std::vector<NotificationManager::Notification> NotificationManager::s_notifications;
std::mutex NotificationManager::s_mutex;
std::atomic_bool NotificationManager::s_enabled = true;
std::atomic_bool NotificationManager::s_hasNotifications = false;
float NotificationManager::s_currentTime = 0.0f;

float NotificationManager::GetTime() noexcept {
    static const auto START_TIME = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - START_TIME).count();
}

void NotificationManager::Initialize() {
    s_enabled.store(ConfigManager::Get().GetBool("Notifications", "enabled", true), std::memory_order_release);
    const std::scoped_lock lock(s_mutex);
    s_notifications.clear();
    s_notifications.reserve(MAX_NOTIFICATIONS);
    s_hasNotifications.store(false, std::memory_order_release);
}

bool NotificationManager::Update() {
    if (!s_enabled.load(std::memory_order_acquire) || !s_hasNotifications.load(std::memory_order_acquire)) return false;

    const std::scoped_lock lock(s_mutex);
    if (s_notifications.empty()) {
        s_hasNotifications.store(false, std::memory_order_release);
        return false;
    }

    s_currentTime = GetTime();
    std::erase_if(s_notifications, [](const Notification& notification) noexcept {
        return s_currentTime - notification.startTime >= notification.duration;
    });
    const bool active = !s_notifications.empty();
    s_hasNotifications.store(active, std::memory_order_release);
    return active;
}

void NotificationManager::CacheLayout(Notification& notification, float maxWidth) noexcept {
    if (notification.layoutWidth == maxWidth) return;

    const float wrapWidth = maxWidth - TEXT_PADDING_X * 2.0f;
    const char* const textBegin = notification.message.data();
    const ImVec2 textSize = ImGui::CalcTextSize(textBegin, textBegin + notification.message.size(), false, wrapWidth);
    notification.textHeight = textSize.y;
    notification.renderWidth = (std::min)(textSize.x + TEXT_PADDING_X * 2.0f, maxWidth);
    notification.renderHeight = textSize.y + TEXT_PADDING_Y * 2.0f;
    notification.layoutWidth = maxWidth;
}

void NotificationManager::Render() {
    if (!s_enabled.load(std::memory_order_acquire) || !s_hasNotifications.load(std::memory_order_acquire)) return;

    const std::scoped_lock lock(s_mutex);
    if (s_notifications.empty()) return;

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    const float maxNotificationWidth = (std::max)(
        TEXT_PADDING_X * 2.0f + 1.0f, (std::min)(viewport->WorkSize.x * 0.35f, MAX_NOTIFICATION_WIDTH)
    );
    std::size_t visibleCount = 0;
    float containerWidth = 0.0f;
    float containerHeight = 0.0f;

    for (auto& notification : s_notifications) {
        if (CalculateAlpha(s_currentTime - notification.startTime, notification.duration) <= 0.0f) continue;
        ++visibleCount;
        CacheLayout(notification, maxNotificationWidth);
        containerWidth = (std::max)(containerWidth, notification.renderWidth);
        containerHeight += notification.renderHeight;
    }
    if (visibleCount == 0) return;

    containerHeight += PADDING * static_cast<float>(visibleCount - 1);
    const ImVec2 containerPos{
        viewport->WorkPos.x + viewport->WorkSize.x - containerWidth - PADDING,
        viewport->WorkPos.y + PADDING,
    };
    constexpr ImGuiWindowFlags CONTAINER_FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    constexpr ImGuiWindowFlags CHILD_FLAGS =
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

    if (ImGui::Begin("##notifications_container", nullptr, CONTAINER_FLAGS)) {
        for (std::size_t index = s_notifications.size(); index-- > 0;) {
            auto& notification = s_notifications[index];
            const float alpha = CalculateAlpha(s_currentTime - notification.startTime, notification.duration);
            if (alpha <= 0.0f) continue;

            const auto withAlpha = [alpha](const ImVec4& color) noexcept {
                return ImVec4(color.x, color.y, color.z, color.w * alpha);
            };
            ImGui::PushStyleColor(ImGuiCol_ChildBg, withAlpha(NOTIFICATION_BG));
            ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(NOTIFICATION_BORDER));
            ImGui::PushStyleColor(ImGuiCol_Text, withAlpha(NOTIFICATION_TEXT));
            ImGui::SetCursorPosX(containerWidth - notification.renderWidth);

            ImGui::PushID(static_cast<int>(index));
            if (ImGui::BeginChild(
                    "##notification", ImVec2(notification.renderWidth, notification.renderHeight), true, CHILD_FLAGS
                )) {
                const float textY = (notification.renderHeight - notification.textHeight) * 0.5f;
                ImGui::SetCursorPos(ImVec2(TEXT_PADDING_X, textY));
                ImGui::PushTextWrapPos(notification.renderWidth - TEXT_PADDING_X);
                ImGui::TextUnformatted(
                    notification.message.data(), notification.message.data() + notification.message.size()
                );
                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(6);
}

void NotificationManager::AddNotification(std::string_view text, std::string_view suffix, float duration) {
    if (!s_enabled.load(std::memory_order_acquire)) return;

    const std::scoped_lock lock(s_mutex);
    if (!s_enabled.load(std::memory_order_relaxed)) return;
    if (s_notifications.size() == MAX_NOTIFICATIONS) s_notifications.erase(s_notifications.begin());

    std::string message;
    message.reserve(text.size() + suffix.size());
    message.append(text);
    message.append(suffix);
    s_notifications.push_back({
        .message = std::move(message),
        .startTime = GetTime(),
        .duration = duration,
    });
    s_hasNotifications.store(true, std::memory_order_release);
}

void NotificationManager::NotifyStateChange(std::string_view actionName, bool enabled) {
    AddNotification(actionName, enabled ? ENABLED_SUFFIX : DISABLED_SUFFIX, DEFAULT_DURATION);
}

void NotificationManager::NotifyAction(std::string_view actionName) {
    AddNotification(actionName, {}, ACTION_DURATION);
}

constexpr float NotificationManager::CalculateAlpha(float elapsed, float duration) noexcept {
    if (elapsed < FADE_IN_DURATION) return elapsed * INV_FADE_IN;
    const float fadeStartTime = duration - FADE_OUT_DURATION;
    if (elapsed > fadeStartTime) return 1.0f - (elapsed - fadeStartTime) * INV_FADE_OUT;
    return 1.0f;
}

void NotificationManager::SetEnabled(bool enabled) {
    if (s_enabled.exchange(enabled, std::memory_order_acq_rel) == enabled) return;

    ConfigManager::Get().SetBool("Notifications", "enabled", enabled);
    ConfigManager::Get().SaveConfig();
    if (!enabled) {
        const std::scoped_lock lock(s_mutex);
        s_notifications.clear();
        s_hasNotifications.store(false, std::memory_order_release);
    }
}
