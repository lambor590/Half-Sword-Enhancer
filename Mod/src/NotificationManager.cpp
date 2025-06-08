#include "NotificationManager.h"
#include "ConfigManager.h"
#include "imgui/imgui.h"

std::deque<Notification> NotificationManager::s_notifications;
bool NotificationManager::s_enabled = true;

void NotificationManager::Initialize() noexcept {
    s_enabled = g_ConfigManager.GetBool("Notifications", "enabled", true);
}

void NotificationManager::Update() noexcept {
    if (!s_enabled) {
        s_notifications.clear();
        return;
    }
    
    const auto now = std::chrono::steady_clock::now();
    while (!s_notifications.empty()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - s_notifications.front().startTime).count() / 1000.0f;
        
        if (elapsed >= s_notifications.front().duration) {
            s_notifications.pop_front();
        } else {
            break;
        }
    }
}

void NotificationManager::Render() noexcept {
    if (!s_enabled || s_notifications.empty()) return;
    
    static const ImGuiViewport* viewport = ImGui::GetMainViewport();
    static constexpr ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    
    const auto now = std::chrono::steady_clock::now();
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 10.0f));
    
    for (size_t i = 0; i < s_notifications.size(); ++i) {
        const auto& notification = s_notifications[i];
        const float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - notification.startTime).count() / 1000.0f;
        
        const float alpha = CalculateAlpha(notification, elapsed);
        if (alpha <= 0.0f) continue;
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
            NOTIFICATION_BG.x, NOTIFICATION_BG.y, NOTIFICATION_BG.z, NOTIFICATION_BG.w * alpha));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
            NOTIFICATION_TEXT.x, NOTIFICATION_TEXT.y, NOTIFICATION_TEXT.z, NOTIFICATION_TEXT.w * alpha));
        
        ImGui::SetNextWindowPos(ImVec2(
            viewport->Pos.x + viewport->Size.x - NOTIFICATION_WIDTH - PADDING,
            viewport->Pos.y + PADDING + (NOTIFICATION_HEIGHT + PADDING) * i
        ));
        ImGui::SetNextWindowSize(ImVec2(NOTIFICATION_WIDTH, NOTIFICATION_HEIGHT));
        
        char windowName[32];
        snprintf(windowName, sizeof(windowName), "##notification_%zu", i);
        
        ImGui::Begin(windowName, nullptr, windowFlags);
        ImGui::TextWrapped("%s", notification.message.c_str());
        ImGui::End();
        
        ImGui::PopStyleColor(2);
    }
    
    ImGui::PopStyleVar(3);
}

void NotificationManager::AddNotification(std::string&& message, float duration) noexcept {
    if (!s_enabled) return;
    
    s_notifications.emplace_back(std::move(message), duration);
    
    while (s_notifications.size() > MAX_NOTIFICATIONS) {
        s_notifications.pop_front();
    }
}

void NotificationManager::NotifyHookToggle(const std::string& functionName, bool enabled) noexcept {
    if (!s_enabled) return;
    
    std::string message;
    message.reserve(functionName.length() + 10);
    message = functionName;
    message += enabled ? " enabled" : " disabled";
    AddNotification(std::move(message));
}

void NotificationManager::NotifyOneTimeAction(const std::string& actionName) noexcept {
    if (!s_enabled) return;
    
    std::string message;
    message.reserve(actionName.length() + 10);
    message = actionName;
    message += " executed";
    AddNotification(std::move(message), 2.0f);
}

float NotificationManager::CalculateAlpha(const Notification& notification, float elapsed) noexcept {
    if (elapsed < Notification::fadeInDuration) {
        return elapsed / Notification::fadeInDuration;
    }
    
    const float fadeStartTime = notification.duration - Notification::fadeOutDuration;
    if (elapsed > fadeStartTime) {
        const float fadeProgress = (elapsed - fadeStartTime) / Notification::fadeOutDuration;
        return 1.0f - fadeProgress;
    }
    
    return 1.0f;
}

void NotificationManager::SetEnabled(bool enabled) noexcept {
    s_enabled = enabled;
    g_ConfigManager.SetBool("Notifications", "enabled", enabled);
    g_ConfigManager.SaveConfig();
    
    if (!enabled) {
        s_notifications.clear();
    }
}

