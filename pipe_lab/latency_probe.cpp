#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "CUESDK.h"
extern "C" void mainCRTStartup(){
 HMODULE dll=LoadLibraryA("original-cue.dll");if(!dll)ExitProcess(10);
 auto handshake=(decltype(&CorsairPerformProtocolHandshake))GetProcAddress(dll,"CorsairPerformProtocolHandshake");
 auto set=(decltype(&CorsairSetLedsColors))GetProcAddress(dll,"CorsairSetLedsColors");
 auto p=handshake();if(!p.serverVersion)ExitProcess(50);
 CorsairLedColor colors[3]={{CLK_Escape,10,20,30},{CLK_G1,0,0,0},{CLK_Mute,0,0,0}};DWORD start=GetTickCount();
 for(int i=0;i<100;++i)if(!set(3,colors))ExitProcess(56);
 DWORD elapsed=GetTickCount()-start;printf("100 synchronous calls: %lu ms\n",elapsed);fflush(stdout);
 ExitProcess(elapsed>1000?60:0);
}
