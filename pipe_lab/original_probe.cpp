#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "CUESDK.h"
static volatile LONG callback_count;
static void callback(void*,bool ok,CorsairError err){printf("callback ok=%d error=%d\n",ok,err);fflush(stdout);InterlockedIncrement(&callback_count);}
extern "C" void mainCRTStartup()
{
 HMODULE dll=LoadLibraryA("original-cue.dll");if(!dll){printf("load-error=%lu\n",GetLastError());ExitProcess(10);}
 auto handshake=(decltype(&CorsairPerformProtocolHandshake))GetProcAddress(dll,"CorsairPerformProtocolHandshake");
 auto error=(decltype(&CorsairGetLastError))GetProcAddress(dll,"CorsairGetLastError");
 auto count=(decltype(&CorsairGetDeviceCount))GetProcAddress(dll,"CorsairGetDeviceCount");
 printf("original-loaded\n");fflush(stdout);
 auto p=handshake();printf("handshake sdk=%s server=%s protocol=%d/%d breaking=%d error=%d\n",p.sdkVersion?p.sdkVersion:"null",p.serverVersion?p.serverVersion:"null",p.sdkProtocolVersion,p.serverProtocolVersion,p.breakingChanges,error());fflush(stdout);
 if(!p.serverVersion||p.serverProtocolVersion!=10||p.breakingChanges||error()!=CE_Success)ExitProcess(50);
 printf("devices=%d error=%d\n",count(),error());fflush(stdout);
 auto info=(decltype(&CorsairGetDeviceInfo))GetProcAddress(dll,"CorsairGetDeviceInfo");
 auto layout=(decltype(&CorsairGetLedPositionsByDeviceIndex))GetProcAddress(dll,"CorsairGetLedPositionsByDeviceIndex");
 auto set=(decltype(&CorsairSetLedsColors))GetProcAddress(dll,"CorsairSetLedsColors");
 auto async=(decltype(&CorsairSetLedsColorsAsync))GetProcAddress(dll,"CorsairSetLedsColorsAsync");
 auto request=(decltype(&CorsairRequestControl))GetProcAddress(dll,"CorsairRequestControl");
 auto release=(decltype(&CorsairReleaseControl))GetProcAddress(dll,"CorsairReleaseControl");
 if(count()!=1)ExitProcess(51);
 auto d=info(0);printf("device ptr=%p type=%d model=%s leds=%d error=%d\n",d,d?d->type:0,d&&d->model?d->model:"null",d?d->ledsCount:0,error());fflush(stdout);
 if(!d||d->type!=CDT_Keyboard||d->ledsCount!=105)ExitProcess(52);
 auto l=layout(0);printf("positions ptr=%p count=%d error=%d\n",l,l?l->numberOfLed:0,error());fflush(stdout);
 if(!l||l->numberOfLed!=105)ExitProcess(53);
 printf("control=%d\n",request(CAM_ExclusiveLightingControl));fflush(stdout);
 CorsairLedColor mixed[3]={{CLK_Escape,10,20,30},{CLK_G1,0,0,0},{CLK_Mute,0,0,0}};
 if(!set(3,mixed))ExitProcess(57);
 CorsairLedColor color={CLK_Escape,10,20,30};printf("set=%d error=%d\n",set(1,&color),error());fflush(stdout);
 printf("async=%d error=%d\n",async(1,&color,callback,nullptr),error());fflush(stdout);
 auto buffer=(decltype(&CorsairSetLedsColorsBufferByDeviceIndex))GetProcAddress(dll,"CorsairSetLedsColorsBufferByDeviceIndex");
 auto flush=(decltype(&CorsairSetLedsColorsFlushBuffer))GetProcAddress(dll,"CorsairSetLedsColorsFlushBuffer");
 auto readback=(decltype(&CorsairGetLedsColors))GetProcAddress(dll,"CorsairGetLedsColors");
 CorsairLedColor two[2]={{CLK_Escape,10,20,30},{CLK_F1,10,20,30}};
 printf("buffer=%d flush=%d\n",buffer(0,2,two),flush());fflush(stdout);
 printf("read=%d colors=%d,%d,%d error=%d\n",readback(2,two),two[0].r,two[0].g,two[0].b,error());fflush(stdout);
 if(!readback(2,two)||two[0].r!=10||two[0].g!=20||two[0].b!=30)ExitProcess(54);
 Sleep(700);if(callback_count!=1)ExitProcess(55);printf("callback_count=%ld release=%d\n",callback_count,release(CAM_ExclusiveLightingControl));fflush(stdout);
 Sleep(500);ExitProcess(0);
}
