#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
static HANDLE in,out,callback,ready;
static DWORD WINAPI output_connect(void* p){HANDLE h=(HANDLE)p;ConnectNamedPipe(h,NULL);if(h==out)SetEvent(ready);return 0;}
static int read_exact(HANDLE h,void* data,DWORD size){DWORD total=0,n;while(total<size){if(!ReadFile(h,(char*)data+total,size-total,&n,NULL)||!n)return 0;total+=n;}return 1;}
static void send(HANDLE h,const char* data,DWORD size){DWORD n;WriteFile(h,&size,4,&n,NULL);WriteFile(h,data,size,&n,NULL);}
void mainCRTStartup(void){
 const wchar_t* base=L"\\\\.\\pipe\\CorsairUtilityEngine\\{4A48E328-2134-4D19-B31F-764ADEFB844C}";
 wchar_t name[256];swprintf(name,256,L"%ls_in",base);
 HANDLE bin=CreateNamedPipeW(name,PIPE_ACCESS_INBOUND,PIPE_TYPE_BYTE|PIPE_WAIT,8,65536,65536,0,NULL);
 swprintf(name,256,L"%ls_out",base);HANDLE bout=CreateNamedPipeW(name,PIPE_ACCESS_OUTBOUND,PIPE_TYPE_BYTE|PIPE_WAIT,8,65536,65536,0,NULL);
 in=CreateNamedPipeW(L"\\\\.\\pipe\\CorsairUtilityEngine\\OpenRGB-lab_in",PIPE_ACCESS_INBOUND,PIPE_TYPE_BYTE|PIPE_WAIT,8,65536,65536,0,NULL);
 out=CreateNamedPipeW(L"\\\\.\\pipe\\CorsairUtilityEngine\\OpenRGB-lab_out",PIPE_ACCESS_OUTBOUND,PIPE_TYPE_BYTE|PIPE_WAIT,8,65536,65536,0,NULL);
 callback=CreateNamedPipeW(L"\\\\.\\pipe\\CorsairUtilityEngine\\OpenRGB-lab_callback",PIPE_ACCESS_OUTBOUND,PIPE_TYPE_BYTE|PIPE_WAIT,8,65536,65536,0,NULL);
 ready=CreateEventA(NULL,TRUE,FALSE,NULL);
 CreateThread(NULL,0,output_connect,bout,0,NULL);CreateThread(NULL,0,output_connect,out,0,NULL);CreateThread(NULL,0,output_connect,callback,0,NULL);
 printf("ready\n");fflush(stdout);ConnectNamedPipe(bin,NULL);
 unsigned char buffer[65536];DWORD size;
 if(!read_exact(bin,&size,4)||size>sizeof(buffer)||!read_exact(bin,buffer,size))ExitProcess(2);
 printf("bootstrap: %.*s\n",(int)size,buffer);fflush(stdout);
 const char* private_name="\\\\.\\pipe\\CorsairUtilityEngine\\OpenRGB-lab";send(bout,private_name,(DWORD)strlen(private_name)+1);
 ConnectNamedPipe(in,NULL);WaitForSingleObject(ready,2000);
 while(read_exact(in,&size,4)&&size<=sizeof(buffer)&&read_exact(in,buffer,size)){
  HANDLE file=CreateFileA("captured.bin",FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,0,NULL);DWORD n;WriteFile(file,&size,4,&n,NULL);WriteFile(file,buffer,size,&n,NULL);CloseHandle(file);
  printf("request %lu bytes: %.*s\n",size,(int)size,buffer);fflush(stdout);
  HANDLE reply=CreateFileA("reply.bin",GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  if(reply!=INVALID_HANDLE_VALUE){unsigned char data[8192];DWORD length=0;ReadFile(reply,data,sizeof(data),&length,NULL);CloseHandle(reply);WriteFile(out,data,length,&n,NULL);}
 }
 ExitProcess(0);
}
