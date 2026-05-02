#include "Windows.h"

#include "Util.h"

static constexpr int FUNC_COUNT = 180;

extern "C" FARPROC originalFuncs[FUNC_COUNT]{};

static HMODULE hOriginalDLL = NULL;

static BOOL CALLBACK FindMainWindow(HWND hWnd, LPARAM lParam) {
    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(hWnd, &windowProcessId);
    if (windowProcessId != GetCurrentProcessId() || !IsWindowVisible(hWnd) || GetWindow(hWnd, GW_OWNER)) return TRUE;

    *reinterpret_cast<HWND*>(lParam) = hWnd;
    return FALSE;
}

static bool HasMainWindow() {
    HWND window = nullptr;
    EnumWindows(FindMainWindow, reinterpret_cast<LPARAM>(&window));
    return window != nullptr;
}

static DWORD WINAPI BootstrapMod(LPVOID) {
    while (!GetModuleHandleA("dxgi.dll") || (!GetModuleHandleA("d3d11.dll") && !GetModuleHandleA("d3d12.dll")) ||
           !HasMainWindow()) {
        Sleep(100);
    }

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
