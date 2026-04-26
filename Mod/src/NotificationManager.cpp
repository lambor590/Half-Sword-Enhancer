#include "NotificationManager.h"
#include "ConfigManager.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <chrono>
#include <string_view>

namespace {
    constexpr std::string_view ENABLED_PREFIX = "Enabled ";
    constexpr std::string_view DISABLED_PREFIX = "Disabled ";
    constexpr std::string_view EXECUTED_PREFIX = "Executed ";
    constexpr ImVec4 NOTIFICATION_BG{0.12f, 0.09f, 0.06f, 0.95f};
    constexpr ImVec4 NOTIFICATION_TEXT{0.95f, 0.92f, 0.85f, 1.0f};
    constexpr ImVec4 NOTIFICATION_BORDER{0.71f, 0.57f, 0.25f, 1.0f};
}

std::vector<Notification> NotificationManager::s_notifications;
bool NotificationManager::s_enabled = true;
float NotificationManager::s_currentTime = 0.0f;


Notification::Notification(std::string&& msg, float dur) noexcept
    : message(std::move(msg)), startTime(NotificationManager::GetTime()), duration(dur) {}

float NotificationManager::GetTime() noexcept {
    if (!s_enabled && s_notifications.empty()) {
        return s_currentTime;
    }

    static auto startTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - startTime).count();
}

void NotificationManager::Initialize() noexcept {
    s_enabled = g_ConfigManager.GetBool("Notifications", "enabled", true);
}

void NotificationManager::Update() noexcept {
    if (!s_enabled) [[unlikely]] {
        if (!s_notifications.empty()) {
            s_notifications.clear();
        }
        return;
    }

    if (s_notifications.empty()) [[likely]]
        return;

    s_currentTime = GetTime();

    std::erase_if(s_notifications, [currentTime = s_currentTime](const Notification& notification) noexcept -> bool {
        return currentTime - notification.startTime >= notification.duration;
    });
}

void NotificationManager::Render() noexcept {
    if (!s_enabled || s_notifications.empty()) [[unlikely]]
        return;

    size_t visibleCount = 0;
    float maxWidth = MIN_NOTIFICATION_WIDTH;

    for (const auto& notification : s_notifications) {
        const float elapsed = s_currentTime - notification.startTime;
        if (CalculateAlpha(elapsed, notification.duration) > 0.0f) {
            ++visibleCount;
            const ImVec2 textSize = ImGui::CalcTextSize(notification.message.c_str());
            const float notifWidth = textSize.x + TEXT_PADDING * 2.0f;
            if (notifWidth > maxWidth) maxWidth = notifWidth;
        }
    }

    if (visibleCount == 0) [[unlikely]]
        return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float containerWidth = maxWidth;
    const ImVec2 containerPos{viewport->Pos.x + viewport->Size.x - containerWidth - PADDING, viewport->Pos.y + PADDING};

    static constexpr ImGuiWindowFlags containerFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    static constexpr ImGuiWindowFlags childFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::SetNextWindowPos(containerPos);
    ImGui::SetNextWindowSize(ImVec2(containerWidth, HEIGHT_PLUS_PADDING * visibleCount - PADDING));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, PADDING));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    if (ImGui::Begin("##notifications_container", nullptr, containerFlags)) [[likely]] {
        for (int i = static_cast<int>(s_notifications.size()) - 1; i >= 0; --i) {
            const auto& notification = s_notifications[i];
            const float elapsed = s_currentTime - notification.startTime;
            const float alpha = CalculateAlpha(elapsed, notification.duration);

            if (alpha <= 0.0f) [[unlikely]]
                continue;

            const ImVec2 textSize = ImGui::CalcTextSize(notification.message.c_str());
            const float notifWidth = (std::max)(textSize.x + TEXT_PADDING * 2.0f, MIN_NOTIFICATION_WIDTH);

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
            if (ImGui::BeginChild("##notification", ImVec2(notifWidth, NOTIFICATION_HEIGHT), true, childFlags))
                [[likely]] {
                const float textY = (NOTIFICATION_HEIGHT - textSize.y) * 0.5f;
                ImGui::SetCursorPos(ImVec2(TEXT_PADDING, textY));
                ImGui::TextUnformatted(
                    notification.message.c_str(), notification.message.c_str() + notification.message.size()
                );
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

void NotificationManager::AddNotification(std::string&& message, float duration) noexcept {
    if (!s_enabled) [[unlikely]]
        return;

    if (s_notifications.size() >= MAX_NOTIFICATIONS) [[unlikely]] {
        s_notifications.erase(s_notifications.begin());
    }

    s_notifications.emplace_back(std::move(message), duration);
}

void NotificationManager::NotifyHookToggle(std::string_view functionName, bool enabled) noexcept {
    if (!s_enabled) [[unlikely]]
        return;

    const std::string_view prefix = enabled ? ENABLED_PREFIX : DISABLED_PREFIX;

    std::string message;
    message.reserve(prefix.size() + functionName.size());
    message.append(prefix);
    message.append(functionName);

    AddNotification(std::move(message));
}

void NotificationManager::NotifyOneTimeAction(std::string_view actionName) noexcept {
    if (!s_enabled) [[unlikely]]
        return;

    static constexpr float actionDuration = 2.0f;

    std::string message;
    message.reserve(EXECUTED_PREFIX.size() + actionName.size());
    message.append(EXECUTED_PREFIX);
    message.append(actionName);

    AddNotification(std::move(message), actionDuration);
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

void NotificationManager::SetEnabled(bool enabled) noexcept {
    if (s_enabled == enabled) return;

    s_enabled = enabled;
    g_ConfigManager.SetBool("Notifications", "enabled", enabled);
    g_ConfigManager.SaveConfig();

    if (!enabled) {
        s_notifications.clear();
        s_notifications.shrink_to_fit();
    }
}
