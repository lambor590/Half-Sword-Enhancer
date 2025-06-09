#include "NotificationManager.h"
#include "ConfigManager.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <chrono>

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
    
    if (s_notifications.empty()) [[likely]] return;
    
    s_currentTime = GetTime();
    
    const auto newEnd = std::remove_if(s_notifications.begin(), s_notifications.end(),
        [currentTime = s_currentTime](const Notification& notification) noexcept -> bool {
            return currentTime - notification.startTime >= notification.duration;
        });
    
    if (newEnd != s_notifications.end()) [[unlikely]] {
        s_notifications.erase(newEnd, s_notifications.end());
    }
}

void NotificationManager::Render() noexcept {
    if (!s_enabled || s_notifications.empty()) [[unlikely]] return;
    
    static size_t lastVisibleCount = 0;
    static float lastCheckTime = 0.0f;
    
    if (s_currentTime - lastCheckTime >= CHECK_INTERVAL) [[unlikely]] {
        lastVisibleCount = 0;
        for (const auto& notification : s_notifications) {
            const float elapsed = s_currentTime - notification.startTime;
            if (CalculateAlpha(elapsed, notification.duration) > 0.0f) [[likely]] {
                ++lastVisibleCount;
            }
        }
        lastCheckTime = s_currentTime;
    }
    
    if (lastVisibleCount == 0) [[unlikely]] return;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 containerPos{
        viewport->Pos.x + viewport->Size.x - NOTIFICATION_WIDTH - PADDING,
        viewport->Pos.y + PADDING
    };
    
    static constexpr ImGuiWindowFlags containerFlags = 
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    static constexpr ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoScrollbar;
    
    ImGui::SetNextWindowPos(containerPos);
    ImGui::SetNextWindowSize(ImVec2(NOTIFICATION_WIDTH, HEIGHT_PLUS_PADDING * lastVisibleCount - PADDING));
    
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
            
            if (alpha <= 0.0f) [[unlikely]] continue;
            
            const float slideAlpha = elapsed < FADE_IN_DURATION ? elapsed * INV_FADE_IN : 1.0f;
            const float slideOffset = (1.0f - slideAlpha) * SLIDE_DISTANCE;
            
            const ImVec4 bgColor(NOTIFICATION_BG.x, NOTIFICATION_BG.y, NOTIFICATION_BG.z, NOTIFICATION_BG.w * alpha);
            const ImVec4 borderColor(NOTIFICATION_BORDER.x, NOTIFICATION_BORDER.y, NOTIFICATION_BORDER.z, NOTIFICATION_BORDER.w * alpha);
            const ImVec4 textColor(NOTIFICATION_TEXT.x, NOTIFICATION_TEXT.y, NOTIFICATION_TEXT.z, NOTIFICATION_TEXT.w * alpha);
            
            ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
            ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
            ImGui::PushStyleColor(ImGuiCol_Text, textColor);
            
            ImGui::SetCursorPosX(slideOffset);
            
            ImGui::PushID(i);
            if (ImGui::BeginChild("##notification", ImVec2(NOTIFICATION_WIDTH - slideOffset, NOTIFICATION_HEIGHT), true, childFlags)) [[likely]] {
                ImGui::SetCursorPos(ImVec2(TEXT_PADDING, TEXT_PADDING));
                ImGui::PushTextWrapPos(TEXT_WRAP_WIDTH - slideOffset);
                ImGui::TextWrapped("%s", notification.message.c_str());
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

void NotificationManager::AddNotification(std::string&& message, float duration) noexcept {
    if (!s_enabled) [[unlikely]] return;
    
    if (s_notifications.size() >= MAX_NOTIFICATIONS) [[unlikely]] {
        s_notifications.erase(s_notifications.begin());
    }
    
    s_notifications.emplace_back(std::move(message), duration);
}

void NotificationManager::NotifyHookToggle(const std::string& functionName, bool enabled) noexcept {
    if (!s_enabled) [[unlikely]] return;
    
    static constexpr auto enabledStr = " enabled";
    static constexpr auto disabledStr = " disabled";
    
    AddNotification(functionName + (enabled ? enabledStr : disabledStr));
}

void NotificationManager::NotifyOneTimeAction(const std::string& actionName) noexcept {
    if (!s_enabled) [[unlikely]] return;
    
    static constexpr auto executedStr = " executed";
    static constexpr float actionDuration = 2.0f;
    
    AddNotification(actionName + executedStr, actionDuration);
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