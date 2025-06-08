#include "NotificationManager.h"
#include "ConfigManager.h"
#include "imgui/imgui.h"
#include <algorithm>

std::vector<std::unique_ptr<Notification>> NotificationManager::s_notifications;
bool NotificationManager::s_enabled = true;
bool NotificationManager::s_initialized = false;

void NotificationManager::Initialize() noexcept {
    if (!s_initialized) {
        s_enabled = g_ConfigManager.GetBool("Notifications", "enabled", true);
        s_initialized = true;
    }
}

void NotificationManager::Update() noexcept {
    if (!s_enabled) {
        s_notifications.clear();
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    s_notifications.erase(
        std::remove_if(s_notifications.begin(), s_notifications.end(),
            [now](const std::unique_ptr<Notification>& notification) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - notification->startTime).count() / 1000.0f;
                return elapsed >= notification->duration;
            }), 
        s_notifications.end()
    );
}

void NotificationManager::Render() noexcept {
    if (!s_enabled || s_notifications.empty()) return;
    
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float padding = 10.0f;
    const float notificationWidth = 300.0f;
    const float notificationHeight = 50.0f;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 10.0f));
    
    for (size_t i = 0; i < s_notifications.size(); ++i) {
        const auto& notification = s_notifications[i];
        float alpha = CalculateAlpha(*notification);
        
        if (alpha <= 0.0f) continue;
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, alpha * 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
        
        ImVec2 pos;
        pos.x = viewport->Pos.x + viewport->Size.x - notificationWidth - padding;
        pos.y = viewport->Pos.y + padding + (notificationHeight + padding) * i;
        
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(ImVec2(notificationWidth, notificationHeight));
        
        std::string windowName = "##notification_" + std::to_string(i);
        ImGui::Begin(windowName.c_str(), nullptr, 
            ImGuiWindowFlags_NoDecoration | 
            ImGuiWindowFlags_NoInputs | 
            ImGuiWindowFlags_NoMove | 
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing);
        
        ImGui::TextWrapped("%s", notification->message.c_str());
        
        ImGui::End();
        ImGui::PopStyleColor(2);
    }
    
    ImGui::PopStyleVar(3);
}

void NotificationManager::AddNotification(const std::string& message, float duration) noexcept {
    if (!s_enabled) return;
    
    s_notifications.push_back(std::make_unique<Notification>(message, duration));
    
    const size_t maxNotifications = 5;
    if (s_notifications.size() > maxNotifications) {
        s_notifications.erase(s_notifications.begin(), 
            s_notifications.begin() + (s_notifications.size() - maxNotifications));
    }
}

void NotificationManager::NotifyHookToggle(const std::string& functionName, bool enabled) noexcept {
    if (!s_enabled) return;
    
    std::string message = functionName + (enabled ? " enabled" : " disabled");
    AddNotification(message, 2.5f);
}

void NotificationManager::NotifyOneTimeAction(const std::string& actionName) noexcept {
    if (!s_enabled) return;
    
    std::string message = actionName + " executed";
    AddNotification(message, 2.0f);
}

void NotificationManager::SetEnabled(bool enabled) noexcept {
    s_enabled = enabled;
    g_ConfigManager.SetBool("Notifications", "enabled", enabled);
    g_ConfigManager.SaveConfig();
    
    if (!enabled) {
        s_notifications.clear();
    }
}

float NotificationManager::CalculateAlpha(const Notification& notification) noexcept {
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - notification.startTime).count() / 1000.0f;
    
    if (elapsed < notification.fadeInDuration) {
        return elapsed / notification.fadeInDuration;
    }
    
    float fadeStartTime = notification.duration - notification.fadeOutDuration;
    if (elapsed > fadeStartTime) {
        float fadeProgress = (elapsed - fadeStartTime) / notification.fadeOutDuration;
        return 1.0f - fadeProgress;
    }
    
    return 1.0f;
}