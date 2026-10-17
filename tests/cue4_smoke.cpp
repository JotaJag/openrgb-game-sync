#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../bridge/sdk4/iCUESDK.h"
static volatile LONG notified,completed;
static void state(void*,const CorsairSessionStateChanged* event){if(event->state==CSS_Connected)InterlockedExchange(&notified,1);}
static void done(void*,CorsairError error){if(error==CE_Success)InterlockedExchange(&completed,1);}
extern "C" void mainCRTStartup()
{
 HMODULE dll=LoadLibraryA("iCUESDK.x64.dll");if(!dll)ExitProcess(40);
#define LOAD(name) auto name=reinterpret_cast<decltype(&::name)>(GetProcAddress(dll,#name));if(!name)ExitProcess(41)
 LOAD(CorsairConnect);LOAD(CorsairGetDevices);LOAD(CorsairGetLedPositions);LOAD(CorsairSetLedColorsBuffer);
 LOAD(CorsairSetLedColorsFlushBufferAsync);LOAD(CorsairGetLedColors);LOAD(CorsairReleaseControl);LOAD(CorsairDisconnect);
 if(CorsairConnect(state,nullptr)!=CE_Success||!notified)ExitProcess(42);
 CorsairDeviceFilter filter{1};CorsairDeviceInfo device;int count=0;
 if(CorsairGetDevices(&filter,1,&device,&count)!=CE_Success||count!=1)ExitProcess(43);
 CorsairLedPosition positions[126];
 if(CorsairGetLedPositions(device.id,126,positions,&count)!=CE_Success||count<90||count>126)ExitProcess(44);
 CorsairLedColor colors[126];for(int i=0;i<count;++i)colors[i]={positions[i].id,10,20,30,255};
 if(CorsairSetLedColorsBuffer(device.id,count,colors)!=CE_Success)ExitProcess(45);
 if(CorsairSetLedColorsFlushBufferAsync(done,nullptr)!=CE_Success)ExitProcess(46);
 for(int i=0;i<60&&!completed;++i)Sleep(10);if(!completed)ExitProcess(47);
 colors[0].r=0;if(CorsairGetLedColors(device.id,1,colors)!=CE_Success||colors[0].r!=10)ExitProcess(48);
 Sleep(700);if(CorsairReleaseControl(device.id)!=CE_Success)ExitProcess(49);
 Sleep(350);CorsairDisconnect();ExitProcess(0);
}
