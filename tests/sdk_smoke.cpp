#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "CUESDK.h"
#include <stdint.h>

#ifdef TEST_CUE
static decltype(&CorsairGetLedsColors) getColors;
static volatile LONG callbackRan;
static bool callbackOK;
static void callback(void*, bool success, CorsairError error)
{
    CorsairLedColor color{CLK_Escape,0,0,0};
    callbackOK = success && error == CE_Success && getColors(1,&color) && color.r == 10 && color.g == 20 && color.b == 30;
    InterlockedExchange(&callbackRan,1);
}
extern "C" void mainCRTStartup()
{
    HMODULE dll = LoadLibraryA(
#ifdef TEST_32
"CUESDK_2015.dll"
#else
"CUESDK.x64_2015.dll"
#endif
);
    if (!dll) ExitProcess(20);
#define LOAD(name) auto name = reinterpret_cast<decltype(&::name)>(GetProcAddress(dll,#name)); if (!name) ExitProcess(21)
    LOAD(CorsairPerformProtocolHandshake); LOAD(CorsairGetDeviceCount); LOAD(CorsairGetDeviceInfo);
    LOAD(CorsairGetLedPositions); LOAD(CorsairSetLedsColorsBufferByDeviceIndex);
    LOAD(CorsairSetLedsColorsFlushBufferAsync); LOAD(CorsairGetLastError);
    LOAD(CorsairRequestControl); LOAD(CorsairReleaseControl); LOAD(CorsairGetLedsColors);
    auto protocol = CorsairPerformProtocolHandshake();
    if (!protocol.serverVersion || protocol.breakingChanges || CorsairGetDeviceCount()!=2 ||
        !CorsairRequestControl(CAM_ExclusiveLightingControl)) ExitProcess(22);
    auto* device = CorsairGetDeviceInfo(0); auto* layout = CorsairGetLedPositions();
    if (!device || !layout || device->ledsCount != layout->numberOfLed || layout->numberOfLed > 126) ExitProcess(23);
    CorsairLedColor colors[126];
    for (int i=0;i<layout->numberOfLed;++i) colors[i] = {layout->pLedPosition[i].ledId,10,20,30};
    if (!CorsairSetLedsColorsBufferByDeviceIndex(0,layout->numberOfLed,colors)) ExitProcess(24);
    CorsairLedColor invalid{CLK_Escape,256,0,0};
    if (CorsairSetLedsColorsBufferByDeviceIndex(0,1,&invalid) || CorsairGetLastError()!=CE_InvalidArguments) ExitProcess(25);
    getColors = CorsairGetLedsColors;
    if (!CorsairSetLedsColorsFlushBufferAsync(callback,nullptr)) ExitProcess(26);
    for (int i=0;i<60 && !InterlockedCompareExchange(&callbackRan,0,0);++i) Sleep(10);
    if (!callbackRan || !callbackOK) ExitProcess(26);
    Sleep(700);
    if (!CorsairReleaseControl(CAM_ExclusiveLightingControl)) ExitProcess(27);
    Sleep(350); ExitProcess(0);
}
#else
extern "C" void mainCRTStartup()
{
    HMODULE dll=LoadLibraryA(
#ifdef TEST_32
"LogitechLed32.dll"
#else
"LogitechLed.dll"
#endif
); if(!dll) ExitProcess(30);
    auto init=(BOOL(*)())GetProcAddress(dll,"LogiLedInit");
    auto shutdown=(void(*)())GetProcAddress(dll,"LogiLedShutdown");
    auto lighting=(BOOL(*)(int,int,int))GetProcAddress(dll,"LogiLedSetLighting");
    auto bitmap=(BOOL(*)(const unsigned char*))GetProcAddress(dll,"LogiLedSetLightingFromBitmap");
    auto setkey=(BOOL(*)(int,int,int,int))GetProcAddress(dll,"LogiLedSetLightingForKeyWithKeyName");
    auto save=(BOOL(*)(int))GetProcAddress(dll,"LogiLedSaveLightingForKey");
    auto restore=(BOOL(*)(int))GetProcAddress(dll,"LogiLedRestoreLightingForKey");
    auto release=(BOOL(*)())GetProcAddress(dll,"LogiLedRestoreLighting");
    if(!init||!shutdown||!lighting||!bitmap||!setkey||!save||!restore||!release||!init()) ExitProcess(31);
    if(!lighting(100,0,0) || lighting(101,0,0)) ExitProcess(32);
    unsigned char pixels[504];
    for(int i=0;i<126;++i){pixels[i*4]=30;pixels[i*4+1]=20;pixels[i*4+2]=10;pixels[i*4+3]=255;}
    if(!bitmap(pixels)||!save(0x10)||!setkey(0x10,0,0,100)) ExitProcess(33);
    Sleep(350);
    if(!restore(0x10)) ExitProcess(34);
    Sleep(350);
    if(!release()) ExitProcess(35);
    Sleep(350); shutdown(); ExitProcess(0);
}
#endif
