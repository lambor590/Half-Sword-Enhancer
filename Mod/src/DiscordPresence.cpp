#include "Utils/DiscordPresence.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <optional>
#include <string_view>

#include "ConfigManager.h"
#include "Hooks/GameHook.h"
#include "Logger.h"
#include "SDK/BP_HalfSwordGameMode_classes.hpp"

namespace {
    using Clock = std::chrono::steady_clock;
    constexpr std::uint64_t APPLICATION_ID = 1440132640333103207ULL;
    constexpr std::string_view COMMUNITY_URL = "https://discord.gg/x3KmgsQYMp";
    constexpr std::string_view DOWNLOAD_URL = "https://halfswordenhancer.com";
    constexpr std::string_view COMMUNITY_IMAGE =
        "https://cdn.discordapp.com/icons/1322288077275795458/fafcb72b235c99eac63bfc9e5f616a79.png";
    constexpr auto UPDATE_INTERVAL = std::chrono::seconds(15);
    constexpr auto REFRESH_INTERVAL = std::chrono::seconds(60);

    // The stable C ABI exported by the game's Discord Social SDK. Handles are
    // opaque; strings use pointer/size and optional strings use a nullable pointer.
    // https://discord.com/developers/docs/social-sdk/discordpp_8h_source.html
    struct Handle { void* value = nullptr; };
    struct Text { const char* data; std::size_t size; };
    using Callback = void (*)(Handle*, void*);
    using Drop = void (*)(Handle*);

    struct Api {
        void (*clientInit)(Handle*);
        Drop clientDrop;
        void (*setApplicationId)(Handle*, std::uint64_t);
        void (*update)(Handle*, Handle*, Callback, void (*)(void*), void*);
        void (*clear)(Handle*);
        void (*runCallbacks)();
        bool (*successful)(const Handle*);
        Drop resultDrop;
        void (*activityInit)(Handle*);
        Drop activityDrop;
        void (*setName)(Handle*, Text);
        void (*setType)(Handle*, int);
        void (*setDetails)(Handle*, const Text*);
        void (*setState)(Handle*, const Text*);
        void (*setTimestamps)(Handle*, const Handle*);
        void (*timestampsInit)(Handle*);
        Drop timestampsDrop;
        void (*setStart)(Handle*, std::uint64_t);
        void (*addButton)(Handle*, const Handle*);
        void (*buttonInit)(Handle*);
        Drop buttonDrop;
        void (*setLabel)(Handle*, Text);
        void (*setUrl)(Handle*, Text);
        void (*assetsInit)(Handle*);
        Drop assetsDrop;
        void (*setAssets)(Handle*, const Handle*);
        void (*setSmallImage)(Handle*, const Text*);
        void (*setSmallText)(Handle*, const Text*);
    };

    struct OwnedHandle {
        Handle handle;
        Drop drop;
        OwnedHandle(void (*init)(Handle*), Drop destroy) : drop(destroy) { init(&handle); }
        ~OwnedHandle() { if (handle.value) drop(&handle); }
        OwnedHandle(const OwnedHandle&) = delete;
        OwnedHandle& operator=(const OwnedHandle&) = delete;
    };

    Logger logger{"DiscordPresence"};
    Api api{};
    Handle client;
    std::atomic<bool> running{false};
    std::atomic<bool> queued{false};
    Clock::time_point nextPoll;
    Clock::time_point nextPublish;
    Clock::time_point lastPublish;
    std::uint64_t sessionStart = 0;
    bool pending = false;
    bool failed = false;
    unsigned int accepted = 0;
    unsigned int rejected = 0;
    HANDLE callbackReleased = nullptr;
    std::optional<DiscordPresence::Activity> published;
    std::optional<DiscordPresence::Activity> submitted;

    Text View(std::string_view text) { return {text.data(), text.size()}; }

    bool CallbackPending() {
        return callbackReleased && WaitForSingleObject(callbackReleased, 0) != WAIT_OBJECT_0;
    }

    template <typename Function>
    bool Load(HMODULE module, Function& function, const char* name) {
        function = reinterpret_cast<Function>(GetProcAddress(module, name));
        return function != nullptr;
    }

    bool LoadApi() {
        if (api.clientInit) return true;
        const auto module = GetModuleHandleW(L"discord_partner_sdk.dll");
        if (!module) return false;
        Api next{};
        if (!(Load(module, next.clientInit, "Discord_Client_Init") &&
              Load(module, next.clientDrop, "Discord_Client_Drop") &&
              Load(module, next.setApplicationId, "Discord_Client_SetApplicationId") &&
              Load(module, next.update, "Discord_Client_UpdateRichPresence") &&
              Load(module, next.clear, "Discord_Client_ClearRichPresence") &&
              Load(module, next.runCallbacks, "Discord_RunCallbacks") &&
              Load(module, next.successful, "Discord_ClientResult_Successful") &&
              Load(module, next.resultDrop, "Discord_ClientResult_Drop") &&
              Load(module, next.activityInit, "Discord_Activity_Init") &&
              Load(module, next.activityDrop, "Discord_Activity_Drop") &&
              Load(module, next.setName, "Discord_Activity_SetName") &&
              Load(module, next.setType, "Discord_Activity_SetType") &&
              Load(module, next.setDetails, "Discord_Activity_SetDetails") &&
              Load(module, next.setState, "Discord_Activity_SetState") &&
              Load(module, next.setTimestamps, "Discord_Activity_SetTimestamps") &&
              Load(module, next.timestampsInit, "Discord_ActivityTimestamps_Init") &&
              Load(module, next.timestampsDrop, "Discord_ActivityTimestamps_Drop") &&
              Load(module, next.setStart, "Discord_ActivityTimestamps_SetStart") &&
              Load(module, next.addButton, "Discord_Activity_AddButton") &&
              Load(module, next.buttonInit, "Discord_ActivityButton_Init") &&
              Load(module, next.buttonDrop, "Discord_ActivityButton_Drop") &&
              Load(module, next.setLabel, "Discord_ActivityButton_SetLabel") &&
              Load(module, next.setUrl, "Discord_ActivityButton_SetUrl") &&
              Load(module, next.assetsInit, "Discord_ActivityAssets_Init") &&
              Load(module, next.assetsDrop, "Discord_ActivityAssets_Drop") &&
              Load(module, next.setAssets, "Discord_Activity_SetAssets") &&
              Load(module, next.setSmallImage, "Discord_ActivityAssets_SetSmallImage") &&
              Load(module, next.setSmallText, "Discord_ActivityAssets_SetSmallText")))
            return false;
        api = next;
        return true;
    }

    void Clear() {
        pending = false;
        published.reset();
        submitted.reset();
        if (client.value) {
            api.clear(&client);
            api.clientDrop(&client);
            client = {};
        }
    }

    void Complete(Handle* result, void*) {
        const bool success = api.successful(result);
        api.resultDrop(result);
        if (!pending) return;
        pending = false;
        if (success) {
            ++accepted;
            published = std::move(submitted);
            if (failed) logger.Log("Presence connection restored");
        } else {
            ++rejected;
            nextPublish = Clock::now() + std::chrono::seconds(30);
            if (!failed) logger.Log("Discord presence unavailable; will retry");
        }
        submitted.reset();
        failed = !success;
    }

    void AddButton(Handle* activity, std::string_view label, std::string_view url) {
        OwnedHandle button(api.buttonInit, api.buttonDrop);
        api.setLabel(&button.handle, View(label));
        api.setUrl(&button.handle, View(url));
        api.addButton(activity, &button.handle);
    }

    void Publish(DiscordPresence::Activity description) {
        if (!callbackReleased) callbackReleased = CreateEventW(nullptr, TRUE, TRUE, nullptr);
        if (!callbackReleased) {
            nextPublish = Clock::now() + std::chrono::seconds(30);
            return;
        }
        OwnedHandle activity(api.activityInit, api.activityDrop);
        api.setName(&activity.handle, View("Half Sword [Enhanced]"));
        api.setType(&activity.handle, 0);
        const auto details = View(description.details);
        const auto state = View(description.state);
        api.setDetails(&activity.handle, &details);
        api.setState(&activity.handle, &state);
        if (description.sessionStart) {
            OwnedHandle timestamps(api.timestampsInit, api.timestampsDrop);
            api.setStart(&timestamps.handle, description.sessionStart);
            api.setTimestamps(&activity.handle, &timestamps.handle);
        }
        OwnedHandle assets(api.assetsInit, api.assetsDrop);
        const auto communityImage = View(COMMUNITY_IMAGE);
        const auto communityText = View("Half Sword Enhancer");
        api.setSmallImage(&assets.handle, &communityImage);
        api.setSmallText(&assets.handle, &communityText);
        api.setAssets(&activity.handle, &assets.handle);
        AddButton(&activity.handle, "Join HSE Discord", COMMUNITY_URL);
        AddButton(&activity.handle, "Get Half Sword Enhancer", DOWNLOAD_URL);

        submitted = std::move(description);
        pending = true;
        ResetEvent(callbackReleased);
        lastPublish = Clock::now();
        nextPublish = lastPublish + UPDATE_INTERVAL;
        // Cancellation can release the callback without invoking Complete. SetEvent
        // uses the same x64 calling convention and lives outside HSE, so signaling
        // completion cannot race the mod's unload with a callback return instruction.
        api.update(&client, &activity.handle, Complete, reinterpret_cast<void (*)(void*)>(&SetEvent), callbackReleased);
    }

    void Tick(const RuntimeContextSnapshot& runtime) {
        if (!running.load(std::memory_order_acquire)) return;
        if (api.runCallbacks) api.runCallbacks();
        auto& config = ConfigManager::Get();
        const auto now = Clock::now();
        if (pending && now - lastPublish >= REFRESH_INTERVAL) {
            Clear();
            nextPublish = now + std::chrono::seconds(30);
        }
        if (CallbackPending() || now < nextPublish) return;
        if (!LoadApi()) {
            nextPublish = now + std::chrono::seconds(30);
            return;
        }
        if (!client.value) {
            api.clientInit(&client);
            if (!client.value) {
                nextPublish = now + std::chrono::seconds(30);
                return;
            }
            api.setApplicationId(&client, APPLICATION_ID);
        }
        auto description = DiscordPresence::Describe(runtime, config.GetBool("Discord", "share_details", true),
                                                     config.GetBool("Discord", "show_elapsed", true));
        if (published && *published == description && !failed && now - lastPublish < REFRESH_INTERVAL) return;
        Publish(std::move(description));
    }
}

DiscordPresence::Activity DiscordPresence::Describe(const RuntimeContextSnapshot& runtime, bool shareDetails, bool showElapsed) {
    Activity activity{"Playing with Half Sword Enhancer", "Join the HSE community", showElapsed ? sessionStart : 0};
    if (!shareDetails) return activity;
    if (!runtime.world) {
        activity.details = "Loading Half Sword";
        return activity;
    }

    std::string map = runtime.world->GetName();
    if (map.starts_with("Map_Menu_") || map.find("MainMenu") != std::string::npos) {
        activity.details = "In the main menu";
        return activity;
    }
    const bool hub = map.find("Hub_") != std::string::npos || map.find("Smithery") != std::string::npos;
    if (map.starts_with("Map_")) map.erase(0, 4);
    if (map == "Hub_Tavern_Frank") map = "Tavern";
    else if (map == "Workshop_Smithery_Map") map = "Smithery";
    else if (map == "Abyss_Map_Open_EA") map = "Abyss";
    else if (map == "Arena_Cutting_Map") map = "Arena";
    std::ranges::replace(map, '_', ' ');
    activity.details = map.empty() ? "Playing Half Sword" : map.substr(0, 100);

    auto* authority = runtime.world->AuthorityGameMode;
    auto* modeClass = SDK::ABP_HalfSwordGameMode_C::StaticClass();
    auto* mode = authority && modeClass && authority->IsA(modeClass)
        ? static_cast<SDK::ABP_HalfSwordGameMode_C*>(authority) : nullptr;

    // Use the game's match counter; avoid scanning actors or counting allies/NPCs.
    if (!hub && mode && mode->Enemy_Count >= 0) {
        activity.state = std::to_string(mode->Enemy_Count) + (mode->Enemy_Count == 1 ? " enemy remaining" : " enemies remaining");
    }
    return activity;
}

void DiscordPresence::Start() noexcept {
    sessionStart = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    nextPoll = {};
    nextPublish = {};
    queued.store(false, std::memory_order_release);
    running.store(true, std::memory_order_release);
}

void DiscordPresence::Poll() {
    const auto now = Clock::now();
    if (!running.load(std::memory_order_acquire) || now < nextPoll || queued.exchange(true)) return;
    nextPoll = now + std::chrono::seconds(1);
    if (!GameHook::QueueAction([](const RuntimeContextSnapshot& runtime) {
            try { Tick(runtime); }
            catch (...) { logger.Log("Presence update failed"); }
            queued.store(false, std::memory_order_release);
        }))
        queued.store(false, std::memory_order_release);
}

void DiscordPresence::Shutdown() noexcept {
    running.store(false, std::memory_order_release);
    Clear();
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    while (CallbackPending() && Clock::now() < deadline) {
        api.runCallbacks();
        WaitForSingleObject(callbackReleased, 1);
    }
    if (CallbackPending()) {
        // An unexpected SDK delay must not leave a callback in unmapped code.
        HMODULE retainedModule = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCWSTR>(&Complete), &retainedModule);
        logger.Log("Discord callback still pending; retaining the mod module until process exit");
    } else if (callbackReleased) {
        CloseHandle(callbackReleased);
        callbackReleased = nullptr;
    }
}

#ifdef HSE_PRIVATE_AUTOMATION
DiscordPresence::Status DiscordPresence::GetStatus() {
    return {client.value != nullptr, pending, accepted, rejected, CallbackPending() ? 1U : 0U, published.value_or(Activity{})};
}
#endif
