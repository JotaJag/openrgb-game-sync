#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef LONG (*InitFn)(void);
typedef LONG (*CreateFn)(int, void*, GUID*);
typedef LONG (*SetFn)(GUID);
void mainCRTStartup(void)
{
    HMODULE dll = LoadLibraryA(
#ifdef TEST_32
"RzChromaSDK.dll"
#else
"RzChromaSDK64.dll"
#endif
);
    if (!dll) ExitProcess(10);
    InitFn init = (InitFn)GetProcAddress(dll, "Init");
    InitFn uninit = (InitFn)GetProcAddress(dll, "UnInit");
    CreateFn create = (CreateFn)GetProcAddress(dll, "CreateChromaLinkEffect");
    SetFn set = (SetFn)GetProcAddress(dll, "SetEffect");
    if (!init || !uninit || !create || !set || init()) ExitProcess(11);
    GUID effect;
    DWORD color = RGB(10, 20, 30);
    if (create(2, &color, &effect) || set(effect)) ExitProcess(12);
    Sleep(700);
    if (uninit()) ExitProcess(13);
    ExitProcess(0);
}
