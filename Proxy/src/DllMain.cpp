#include "Windows.h"

static constexpr int FUNC_COUNT = 180;

extern "C" FARPROC originalFuncs[FUNC_COUNT]{};

static HMODULE hOriginalDLL = NULL;

namespace {
    HMODULE LoadOriginalDLL() {
        char systemPath[MAX_PATH];
        GetSystemDirectoryA(systemPath, MAX_PATH);
        char originalDllPath[MAX_PATH];
        wsprintfA(originalDllPath, "%s\\winmm.dll", systemPath);
        return LoadLibraryA(originalDllPath);
    }

    bool LoadModDLL() {
        HMODULE hModDLL = LoadLibraryA("HSEnhancer.dll");
        if (hModDLL) {
            using InitFn = void (*)();
            auto init = reinterpret_cast<InitFn>(GetProcAddress(hModDLL, "HSE_Initialize"));
            if (init) init();
            return true;
        }

        MessageBoxA(
            NULL,
            "Could not find 'HSEnhancer.dll'."
            "\n\nPlease make sure the file is named 'HSEnhancer.dll' and is in the same folder as the game.",
            "Half Sword Enhancer", MB_OK | MB_ICONINFORMATION
        );
        return false;
    }
}

static DWORD WINAPI BootstrapMod(LPVOID) {
    LoadModDLL();
    return 0;
}

// clang-format off
static constexpr const char* kFuncNames[FUNC_COUNT] =
    {"CloseDriver",
     "DefDriverProc",
     "DriverCallback",
     "DrvGetModuleHandle",
     "GetDriverModuleHandle",
     "OpenDriver",
     "PlaySound",
     "PlaySoundA",
     "PlaySoundW",
     "SendDriverMessage",
     "WOWAppExit",
     "auxGetDevCapsA",
     "auxGetDevCapsW",
     "auxGetNumDevs",
     "auxGetVolume",
     "auxOutMessage",
     "auxSetVolume",
     "joyConfigChanged",
     "joyGetDevCapsA",
     "joyGetDevCapsW",
     "joyGetNumDevs",
     "joyGetPos",
     "joyGetPosEx",
     "joyGetThreshold",
     "joyReleaseCapture",
     "joySetCapture",
     "joySetThreshold",
     "mciDriverNotify",
     "mciDriverYield",
     "mciExecute",
     "mciFreeCommandResource",
     "mciGetCreatorTask",
     "mciGetDeviceIDA",
     "mciGetDeviceIDFromElementIDA",
     "mciGetDeviceIDFromElementIDW",
     "mciGetDeviceIDW",
     "mciGetDriverData",
     "mciGetErrorStringA",
     "mciGetErrorStringW",
     "mciGetYieldProc",
     "mciLoadCommandResource",
     "mciSendCommandA",
     "mciSendCommandW",
     "mciSendStringA",
     "mciSendStringW",
     "mciSetDriverData",
     "mciSetYieldProc",
     "midiConnect",
     "midiDisconnect",
     "midiInAddBuffer",
     "midiInClose",
     "midiInGetDevCapsA",
     "midiInGetDevCapsW",
     "midiInGetErrorTextA",
     "midiInGetErrorTextW",
     "midiInGetID",
     "midiInGetNumDevs",
     "midiInMessage",
     "midiInOpen",
     "midiInPrepareHeader",
     "midiInReset",
     "midiInStart",
     "midiInStop",
     "midiInUnprepareHeader",
     "midiOutCacheDrumPatches",
     "midiOutCachePatches",
     "midiOutClose",
     "midiOutGetDevCapsA",
     "midiOutGetDevCapsW",
     "midiOutGetErrorTextA",
     "midiOutGetErrorTextW",
     "midiOutGetID",
     "midiOutGetNumDevs",
     "midiOutGetVolume",
     "midiOutLongMsg",
     "midiOutMessage",
     "midiOutOpen",
     "midiOutPrepareHeader",
     "midiOutReset",
     "midiOutSetVolume",
     "midiOutShortMsg",
     "midiOutUnprepareHeader",
     "midiStreamClose",
     "midiStreamOpen",
     "midiStreamOut",
     "midiStreamPause",
     "midiStreamPosition",
     "midiStreamProperty",
     "midiStreamRestart",
     "midiStreamStop",
     "mixerClose",
     "mixerGetControlDetailsA",
     "mixerGetControlDetailsW",
     "mixerGetDevCapsA",
     "mixerGetDevCapsW",
     "mixerGetID",
     "mixerGetLineControlsA",
     "mixerGetLineControlsW",
     "mixerGetLineInfoA",
     "mixerGetLineInfoW",
     "mixerGetNumDevs",
     "mixerMessage",
     "mixerOpen",
     "mixerSetControlDetails",
     "mmDrvInstall",
     "mmGetCurrentTask",
     "mmTaskBlock",
     "mmTaskCreate",
     "mmTaskSignal",
     "mmTaskYield",
     "mmioAdvance",
     "mmioAscend",
     "mmioClose",
     "mmioCreateChunk",
     "mmioDescend",
     "mmioFlush",
     "mmioGetInfo",
     "mmioInstallIOProcA",
     "mmioInstallIOProcW",
     "mmioOpenA",
     "mmioOpenW",
     "mmioRead",
     "mmioRenameA",
     "mmioRenameW",
     "mmioSeek",
     "mmioSendMessage",
     "mmioSetBuffer",
     "mmioSetInfo",
     "mmioStringToFOURCCA",
     "mmioStringToFOURCCW",
     "mmioWrite",
     "mmsystemGetVersion",
     "sndPlaySoundA",
     "sndPlaySoundW",
     "timeBeginPeriod",
     "timeEndPeriod",
     "timeGetDevCaps",
     "timeGetSystemTime",
     "timeGetTime",
     "timeKillEvent",
     "timeSetEvent",
     "waveInAddBuffer",
     "waveInClose",
     "waveInGetDevCapsA",
     "waveInGetDevCapsW",
     "waveInGetErrorTextA",
     "waveInGetErrorTextW",
     "waveInGetID",
     "waveInGetNumDevs",
     "waveInGetPosition",
     "waveInMessage",
     "waveInOpen",
     "waveInPrepareHeader",
     "waveInReset",
     "waveInStart",
     "waveInStop",
     "waveInUnprepareHeader",
     "waveOutBreakLoop",
     "waveOutClose",
     "waveOutGetDevCapsA",
     "waveOutGetDevCapsW",
     "waveOutGetErrorTextA",
     "waveOutGetErrorTextW",
     "waveOutGetID",
     "waveOutGetNumDevs",
     "waveOutGetPitch",
     "waveOutGetPlaybackRate",
     "waveOutGetPosition",
     "waveOutGetVolume",
     "waveOutMessage",
     "waveOutOpen",
     "waveOutPause",
     "waveOutPrepareHeader",
     "waveOutReset",
     "waveOutRestart",
     "waveOutSetPitch",
     "waveOutSetPlaybackRate",
     "waveOutSetVolume",
     "waveOutUnprepareHeader",
     "waveOutWrite"};
// clang-format on

static void CacheOriginalFunctions() {
    for (int i = 0; i < FUNC_COUNT; i++)
        originalFuncs[i] = GetProcAddress(hOriginalDLL, kFuncNames[i]);
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        hOriginalDLL = LoadOriginalDLL();
        CacheOriginalFunctions();
        HANDLE bootstrapThread = CreateThread(nullptr, 0, BootstrapMod, nullptr, 0, nullptr);
        if (bootstrapThread) CloseHandle(bootstrapThread);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (hOriginalDLL) FreeLibrary(hOriginalDLL);
    }
    return TRUE;
}
