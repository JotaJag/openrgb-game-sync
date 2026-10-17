// Standalone Wine service. Never loaded into a game process.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LIMIT 65536
static CRITICAL_SECTION host_lock;
static SOCKET host_socket;
static volatile LONG next_id,clients;
static int read_exact(HANDLE h,void* data,DWORD size){DWORD n,total=0;while(total<size){if(!ReadFile(h,(char*)data+total,size-total,&n,NULL)||!n)return 0;total+=n;}return 1;}
static int write_exact(HANDLE h,const void* data,DWORD size){DWORD n,total=0;while(total<size){if(!WriteFile(h,(const char*)data+total,size-total,&n,NULL)||!n)return 0;total+=n;}return 1;}
static int read_message(HANDLE h,char* data,DWORD* size){if(!read_exact(h,size,4)||!*size||*size>=LIMIT||!read_exact(h,data,*size))return 0;data[*size]=0;return 1;}
static int write_message(HANDLE h,const char* data,DWORD size){return write_exact(h,&size,4)&&write_exact(h,data,size);}
static int socket_read(void* data,DWORD size){DWORD total=0;int n;while(total<size){n=recv(host_socket,(char*)data+total,size-total,0);if(n<=0)return 0;total+=n;}return 1;}
static int socket_write(const void* data,DWORD size){DWORD total=0;int n;while(total<size){n=send(host_socket,(const char*)data+total,size-total,0);if(n<=0)return 0;total+=n;}return 1;}
static int backend(const char* request,char* reply,DWORD* size){
 int ok;DWORD n=(DWORD)strlen(request),magic=0x5047534f;
 char* packet=HeapAlloc(GetProcessHeap(),0,n+8);if(!packet)return 0;
 memcpy(packet,&magic,4);memcpy(packet+4,&n,4);memcpy(packet+8,request,n);
 EnterCriticalSection(&host_lock);
 ok=socket_write(packet,n+8)&&socket_read(size,4)&&*size<LIMIT&&socket_read(reply,*size);
 if(ok)reply[*size]=0;
 LeaveCriticalSection(&host_lock);HeapFree(GetProcessHeap(),0,packet);return ok;
}
static DWORD WINAPI focus_worker(void* unused){(void)unused;char message[128],*reply=HeapAlloc(GetProcessHeap(),0,LIMIT+1);DWORD size,pid;while(1){pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);snprintf(message,sizeof(message),"{\"event\":\"focus\",\"pid\":%lu}",pid);if(!backend(message,reply,&size))ExitProcess(3);Sleep(200);}return 0;}
typedef struct {HANDLE in,out,callback;DWORD pid,id;char base[256];} Session;
static HANDLE pipe_for(const wchar_t* name,DWORD access){return CreateNamedPipeW(name,access,PIPE_TYPE_BYTE|PIPE_WAIT,16,LIMIT,LIMIT,0,NULL);}
static DWORD WINAPI connect_callback(void* value){ConnectNamedPipe((HANDLE)value,NULL);return 0;}
static DWORD WINAPI session_worker(void* value){
 Session* s=value;char *request=HeapAlloc(GetProcessHeap(),0,LIMIT+1),*reply=HeapAlloc(GetProcessHeap(),0,LIMIT+1),*envelope=HeapAlloc(GetProcessHeap(),0,LIMIT+256);DWORD size,n;
 if(!request||!reply||!envelope)goto done;
 ConnectNamedPipe(s->out,NULL);HANDLE callback_thread=CreateThread(NULL,0,connect_callback,s->callback,0,NULL);if(callback_thread)CloseHandle(callback_thread);ConnectNamedPipe(s->in,NULL);
 while(read_message(s->in,request,&size)){
  if(size>LIMIT-256)break;
  if(request[size-1]==0)request[size-1]=0;
  n=(DWORD)snprintf(envelope,LIMIT+256,"{\"event\":\"request\",\"client\":%lu,\"pid\":%lu,\"request\":%s}",s->id,s->pid,request);
  if(n>=LIMIT||!backend(envelope,reply,&size)||!write_message(s->out,reply,size+1))break;
 }
 snprintf(envelope,LIMIT+256,"{\"event\":\"disconnect\",\"client\":%lu}",s->id);backend(envelope,reply,&size);
 done:
 CancelIoEx(s->callback,NULL);DisconnectNamedPipe(s->callback);CloseHandle(s->callback);
 DisconnectNamedPipe(s->in);DisconnectNamedPipe(s->out);CloseHandle(s->in);CloseHandle(s->out);
 if(request)HeapFree(GetProcessHeap(),0,request);if(reply)HeapFree(GetProcessHeap(),0,reply);if(envelope)HeapFree(GetProcessHeap(),0,envelope);
 HeapFree(GetProcessHeap(),0,s);InterlockedDecrement(&clients);return 0;
}
void mainCRTStartup(void){
 WSADATA wsa;if(WSAStartup(MAKEWORD(2,2),&wsa))ExitProcess(7);
 char port[16]={0},token[40]={0};GetEnvironmentVariableA("OPENRGB_CUE_PIPE_PORT",port,sizeof(port));GetEnvironmentVariableA("OPENRGB_CUE_PIPE_TOKEN",token,sizeof(token));
 struct sockaddr_in address;memset(&address,0,sizeof(address));address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons((u_short)strtoul(port,NULL,10));
 host_socket=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(host_socket==INVALID_SOCKET||connect(host_socket,(struct sockaddr*)&address,sizeof(address)))ExitProcess(8);
 int no_delay=1;setsockopt(host_socket,IPPROTO_TCP,TCP_NODELAY,(const char*)&no_delay,sizeof(no_delay));
 DWORD io_timeout=1000;setsockopt(host_socket,SOL_SOCKET,SO_RCVTIMEO,(const char*)&io_timeout,sizeof(io_timeout));setsockopt(host_socket,SOL_SOCKET,SO_SNDTIMEO,(const char*)&io_timeout,sizeof(io_timeout));
 if(strlen(token)!=32||!socket_write(token,32))ExitProcess(9);
 InitializeCriticalSection(&host_lock);
 CreateThread(NULL,0,focus_worker,NULL,0,NULL);
 const wchar_t* base=L"\\\\.\\pipe\\CorsairUtilityEngine\\{4A48E328-2134-4D19-B31F-764ADEFB844C}";
 while(1){
  wchar_t name[256];swprintf(name,256,L"%ls_in",base);HANDLE in=pipe_for(name,PIPE_ACCESS_INBOUND);
  swprintf(name,256,L"%ls_out",base);HANDLE out=pipe_for(name,PIPE_ACCESS_OUTBOUND);
  if(in==INVALID_HANDLE_VALUE||out==INVALID_HANDLE_VALUE)ExitProcess(4);
  ConnectNamedPipe(in,NULL);ConnectNamedPipe(out,NULL);
  char greeting[128];DWORD size;
  if(!read_message(in,greeting,&size)||strncmp(greeting,":pid:",5)||clients>=16){CloseHandle(in);CloseHandle(out);continue;}
  DWORD pid=strtoul(greeting+5,NULL,10);if(!pid){CloseHandle(in);CloseHandle(out);continue;}
  Session* s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(Session));if(!s)ExitProcess(5);
  s->pid=pid;s->id=(DWORD)InterlockedIncrement(&next_id);
  snprintf(s->base,sizeof(s->base),"\\\\.\\pipe\\CorsairUtilityEngine\\OpenRGB-%lu-%lu",GetCurrentProcessId(),s->id);
  swprintf(name,256,L"%hs_in",s->base);s->in=pipe_for(name,PIPE_ACCESS_INBOUND);
  swprintf(name,256,L"%hs_out",s->base);s->out=pipe_for(name,PIPE_ACCESS_OUTBOUND);
  swprintf(name,256,L"%hs_callback",s->base);s->callback=pipe_for(name,PIPE_ACCESS_OUTBOUND);
  InterlockedIncrement(&clients);HANDLE worker=CreateThread(NULL,0,session_worker,s,0,NULL);if(!worker)ExitProcess(6);CloseHandle(worker);
  write_message(out,s->base,(DWORD)strlen(s->base)+1);FlushFileBuffers(out);CloseHandle(in);CloseHandle(out);
 }
}
