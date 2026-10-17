#include <windows.h>
#include <stdint.h>
#include "../bridge/sdk4/iCUESDK.h"
static void state(void*,const CorsairSessionStateChanged*){}
extern "C" void mainCRTStartup()
{
    HMODULE chroma=LoadLibraryA("RzChromaSDK64.dll");if(!chroma)ExitProcess(60);
    auto init=(LONG(*)())GetProcAddress(chroma,"Init");
    auto keyboard=(LONG(*)(int,void*,GUID*))GetProcAddress(chroma,"CreateKeyboardEffect");
    auto uninit=(LONG(*)())GetProcAddress(chroma,"UnInit");
    if(!init||!keyboard||!uninit||init())ExitProcess(61);
    uint32_t colors[192+132];
    for(int i=0;i<324;++i)colors[i]=i<192?0x123456:0;
    colors[192]=0x010000ff;
    if(keyboard(9,colors,nullptr))ExitProcess(62);
    Sleep(300);uninit();Sleep(100);
    HMODULE cue=LoadLibraryA("iCUESDK.x64.dll");if(!cue)ExitProcess(63);
#define LOAD(name) auto name=reinterpret_cast<decltype(&::name)>(GetProcAddress(cue,#name));if(!name)ExitProcess(64)
    LOAD(CorsairConnect);LOAD(CorsairGetDevices);LOAD(CorsairGetLedPositions);
    LOAD(CorsairSetLedColors);LOAD(CorsairReleaseControl);LOAD(CorsairDisconnect);
    if(CorsairConnect(state,nullptr)!=CE_Success)ExitProcess(65);
    CorsairDeviceFilter filter{CDT_Mouse};CorsairDeviceInfo mouse;int count;
    if(CorsairGetDevices(&filter,1,&mouse,&count)!=CE_Success||count!=1)ExitProcess(66);
    CorsairLedPosition positions[4];
    if(CorsairGetLedPositions(mouse.id,4,positions,&count)!=CE_Success||count!=4)ExitProcess(67);
    CorsairLedColor pixels[4];for(int i=0;i<4;++i)pixels[i]={positions[i].id,10,20,30,255};
    if(CorsairSetLedColors(mouse.id,4,pixels)!=CE_Success)ExitProcess(68);
    Sleep(300);CorsairReleaseControl(mouse.id);CorsairDisconnect();Sleep(100);
    HMODULE logi=LoadLibraryA("LogitechLed.dll");if(!logi)ExitProcess(69);
    auto linit=(BOOL(*)())GetProcAddress(logi,"LogiLedInit");
    auto lighting=(BOOL(*)(int,int,int))GetProcAddress(logi,"LogiLedSetLighting");
    auto flash=(BOOL(*)(int,int,int,int,int))GetProcAddress(logi,"LogiLedFlashLighting");
    auto pulse=(BOOL(*)(int,int,int,int,int,int,int,int,BOOL))GetProcAddress(logi,"LogiLedPulseSingleKey");
    auto restore=(BOOL(*)())GetProcAddress(logi,"LogiLedRestoreLighting");
    auto shutdown=(void(*)())GetProcAddress(logi,"LogiLedShutdown");
    if(!linit||!lighting||!flash||!pulse||!restore||!shutdown||!linit())ExitProcess(70);
    if(!lighting(0,0,100)||!flash(100,0,0,240,100))ExitProcess(71);
    Sleep(320);
    if(!pulse(1,0,0,100,0,100,0,240,FALSE))ExitProcess(72);
    Sleep(320);restore();shutdown();Sleep(100);ExitProcess(0);
}
