#include "Windows.h"

#include "Util.h"

HMODULE hOriginalDLL = NULL;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        hOriginalDLL = LoadOriginalDLL();
        LoadModDLL();
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (hOriginalDLL) {
            FreeLibrary(hOriginalDLL);
        }
    }
    return TRUE;
}

#define IMPLEMENT_FORWARDED_FUNCTION(name) \
    extern "C" __declspec(dllexport) FARPROC name() \
    { \
        return GetProcAddress(hOriginalDLL, #name); \
    }
IMPLEMENT_FORWARDED_FUNCTION(DwmAttachMilContent)
IMPLEMENT_FORWARDED_FUNCTION(DwmDefWindowProc)
IMPLEMENT_FORWARDED_FUNCTION(DwmDetachMilContent)
IMPLEMENT_FORWARDED_FUNCTION(DwmEnableBlurBehindWindow)
IMPLEMENT_FORWARDED_FUNCTION(DwmEnableComposition)
IMPLEMENT_FORWARDED_FUNCTION(DwmEnableMMCSS)
IMPLEMENT_FORWARDED_FUNCTION(DwmExtendFrameIntoClientArea)
IMPLEMENT_FORWARDED_FUNCTION(DwmFlush)
IMPLEMENT_FORWARDED_FUNCTION(DwmGetColorizationColor)
IMPLEMENT_FORWARDED_FUNCTION(DwmGetCompositionTimingInfo)
IMPLEMENT_FORWARDED_FUNCTION(DwmGetTransportAttributes)
IMPLEMENT_FORWARDED_FUNCTION(DwmGetWindowAttribute)
IMPLEMENT_FORWARDED_FUNCTION(DwmIsCompositionEnabled)
IMPLEMENT_FORWARDED_FUNCTION(DwmModifyPreviousDxFrameDuration)
IMPLEMENT_FORWARDED_FUNCTION(DwmQueryThumbnailSourceSize)
IMPLEMENT_FORWARDED_FUNCTION(DwmRegisterThumbnail)
IMPLEMENT_FORWARDED_FUNCTION(DwmSetDxFrameDuration)
IMPLEMENT_FORWARDED_FUNCTION(DwmSetPresentParameters)
IMPLEMENT_FORWARDED_FUNCTION(DwmSetWindowAttribute)
IMPLEMENT_FORWARDED_FUNCTION(DwmUnregisterThumbnail)
IMPLEMENT_FORWARDED_FUNCTION(DwmUpdateThumbnailProperties)