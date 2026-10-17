#define ICUESDK_EXPORTS
#include "sdk4/iCUESDK.h"
#include "sdk4/iCUESDKLedIdEnum.h"
#include "transport.h"
#include "keyboard4_map.h"
#include <string.h>
static SRWLOCK guard=SRWLOCK_INIT;
static int connected;
static uint32_t colors[2][126];
static bool dirty[2];
static const char* device_ids[]={"OpenRGB.GameSync.Keyboard", "OpenRGB.GameSync.Mouse"};
static const CorsairDeviceType device_types[]={CDT_Keyboard, CDT_Mouse};
static int device_index(const char* id){if(id)for(int i=0;i<2;++i)if(!strcmp(id,device_ids[i]))return i;return -1;}
static unsigned led_id(int device,int index){return device==0?cue4_keys[index]:((unsigned(CLG_Mouse)<<16)|unsigned(index+1));}
static int slots(int device){return device==0?126:4;}
static int led_index(int device,unsigned id){for(int i=0;i<slots(device);++i)if(id&&led_id(device,i)==id)return i;return -1;}
static int device_led_count(int device){int n=0;for(int i=0;i<slots(device);++i)n+=led_id(device,i)!=0;return n;}
static int emit_device(int device){return gs_frame(device==0?1:2,device==0?6:1,device==0?21:4,colors[device]);}
static CorsairSessionStateChangedHandler session_callback;
static void* session_context;
static CorsairSessionDetails details={{4,0,84},{4,0,84},{5,0,0}};

struct Callback { CorsairAsyncCallback callback;void* context;CorsairError error; };
static DWORD WINAPI async_result(void* value){auto* d=(Callback*)value;d->callback(d->context,d->error);HeapFree(GetProcessHeap(),0,d);return 0;}
static CorsairError buffer(const char* id,int size,const CorsairLedColor* data)
{
 if(!connected)return CE_NotConnected;
 int device=device_index(id);if(device<0)return CE_DeviceNotFound;
 if(size<0||size>512||(size&&!data))return CE_InvalidArguments;
 for(int i=0;i<size;++i)if(led_index(device,data[i].id)<0)return CE_InvalidArguments;
 for(int i=0;i<size;++i){auto c=data[i];int ix=led_index(device,c.id);uint32_t previous=colors[device][ix];
  unsigned r=((previous&255)*(255-c.a)+c.r*c.a)/255;
  unsigned g=(((previous>>8)&255)*(255-c.a)+c.g*c.a)/255;
  unsigned b=(((previous>>16)&255)*(255-c.a)+c.b*c.a)/255;
  colors[device][ix]=r|(g<<8)|(b<<16);
 }
 if(size)dirty[device]=true;
 return CE_Success;
}
CorsairError CorsairConnect(CorsairSessionStateChangedHandler callback,void* context)
{
 if(!callback)return CE_InvalidArguments;
 AcquireSRWLockExclusive(&guard);
 if(connected){ReleaseSRWLockExclusive(&guard);return CE_InvalidOperation;}
 connected=gs_init(2,(const void*)(uintptr_t)&CorsairConnect);
 memset(colors,0,sizeof(colors));memset(dirty,0,sizeof(dirty));
 session_callback=callback;session_context=context;bool ok=connected;
 ReleaseSRWLockExclusive(&guard);
 if(!ok)return CE_NotConnected;
 CorsairSessionStateChanged event={CSS_Connected,details};callback(context,&event);return CE_Success;
}
CorsairError CorsairGetSessionDetails(CorsairSessionDetails* value){if(!value)return CE_InvalidArguments;*value=details;return CE_Success;}
CorsairError CorsairDisconnect()
{
 AcquireSRWLockExclusive(&guard);connected=0;auto cb=session_callback;void* ctx=session_context;session_callback=nullptr;
 ReleaseSRWLockExclusive(&guard);gs_shutdown();
 if(cb){CorsairSessionStateChanged event={CSS_Closed,details};cb(ctx,&event);}return CE_Success;
}
CorsairError CorsairGetDeviceInfo(const CorsairDeviceId id,CorsairDeviceInfo* info)
{
 AcquireSRWLockShared(&guard);CorsairError rc=CE_Success;
 int device=device_index(id);
 if(!connected)rc=CE_NotConnected;else if(!info)rc=CE_InvalidArguments;else if(device<0)rc=CE_DeviceNotFound;
 else{memset(info,0,sizeof(*info));info->type=device_types[device];strcpy(info->id,device_ids[device]);
  strcpy(info->model,device==0?"OpenRGB virtual keyboard":"OpenRGB virtual mouse");info->ledCount=device_led_count(device);}
 ReleaseSRWLockShared(&guard);return rc;
}
CorsairError CorsairGetDevices(const CorsairDeviceFilter* filter,int max,CorsairDeviceInfo* devices,int* size)
{
 AcquireSRWLockShared(&guard);CorsairError rc=CE_Success;
 if(!connected)rc=CE_NotConnected;
 else if(!filter||!size||max<0||(max&&!devices))rc=CE_InvalidArguments;
 else{
  *size=0;
  for(int device=0;device<2&&*size<max;++device)if(filter->deviceTypeMask&device_types[device]){
   auto* info=&devices[(*size)++];memset(info,0,sizeof(*info));info->type=device_types[device];
   strcpy(info->id,device_ids[device]);strcpy(info->model,device==0?"OpenRGB virtual keyboard":"OpenRGB virtual mouse");
   info->ledCount=device_led_count(device);
  }
 }
 ReleaseSRWLockShared(&guard);return rc;
}
CorsairError CorsairGetLedPositions(const CorsairDeviceId id,int max,CorsairLedPosition* positions,int* size)
{
 AcquireSRWLockShared(&guard);CorsairError rc=CE_Success;int device=device_index(id);
 if(!connected)rc=CE_NotConnected;else if(device<0)rc=CE_DeviceNotFound;
 else if(!size||max<0||(max&&!positions))rc=CE_InvalidArguments;
 else{*size=0;for(int i=0;i<slots(device)&&*size<max;++i)if(led_id(device,i)){
  positions[*size]={led_id(device,i),double(i%21)*19,double(i/21)*19};++*size;
 }}
 ReleaseSRWLockShared(&guard);return rc;
}
CorsairError CorsairSetLedColorsBuffer(const CorsairDeviceId id,int size,const CorsairLedColor* data)
{AcquireSRWLockExclusive(&guard);auto rc=buffer(id,size,data);ReleaseSRWLockExclusive(&guard);return rc;}
CorsairError CorsairSetLedColors(const CorsairDeviceId id,int size,const CorsairLedColor* data)
{AcquireSRWLockExclusive(&guard);auto rc=buffer(id,size,data);if(rc==CE_Success&&size){int device=device_index(id);if(!emit_device(device))rc=CE_NotConnected;else dirty[device]=false;}ReleaseSRWLockExclusive(&guard);return rc;}
CorsairError CorsairSetLedColorsFlushBufferAsync(CorsairAsyncCallback callback,void* context)
{
 if(!callback)return CE_InvalidArguments;
 AcquireSRWLockExclusive(&guard);CorsairError rc=connected?CE_Success:CE_NotConnected;
 if(connected)for(int device=0;device<2;++device)if(dirty[device]){
  if(emit_device(device))dirty[device]=false;else rc=CE_NotConnected;
 }
 ReleaseSRWLockExclusive(&guard);
 auto* data=(Callback*)HeapAlloc(GetProcessHeap(),0,sizeof(Callback));if(!data)return CE_InvalidOperation;
 *data={callback,context,rc};if(!QueueUserWorkItem(async_result,data,WT_EXECUTEDEFAULT)){HeapFree(GetProcessHeap(),0,data);return CE_InvalidOperation;}return rc;
}
CorsairError CorsairGetLedColors(const CorsairDeviceId id,int size,CorsairLedColor* data)
{
 AcquireSRWLockShared(&guard);CorsairError rc=CE_Success;int device=device_index(id);
 if(!connected)rc=CE_NotConnected;else if(device<0)rc=CE_DeviceNotFound;
 else if(size<0||size>512||(size&&!data))rc=CE_InvalidArguments;
 else{
  for(int i=0;i<size;++i)if(led_index(device,data[i].id)<0)rc=CE_InvalidArguments;
  if(rc==CE_Success)for(int i=0;i<size;++i){auto c=colors[device][led_index(device,data[i].id)];data[i].r=c&255;data[i].g=(c>>8)&255;data[i].b=(c>>16)&255;data[i].a=255;}
 }
 ReleaseSRWLockShared(&guard);return rc;
}
CorsairError CorsairSetLayerPriority(unsigned priority){return !connected?CE_NotConnected:priority>255?CE_InvalidArguments:CE_Success;}
CorsairError CorsairRequestControl(const CorsairDeviceId id,CorsairAccessLevel level){return !connected?CE_NotConnected:device_index(id)<0?CE_DeviceNotFound:level==CAL_Shared||level==CAL_ExclusiveLightingControl?CE_Success:CE_NotAllowed;}
CorsairError CorsairReleaseControl(const CorsairDeviceId id)
{
 AcquireSRWLockExclusive(&guard);int device=device_index(id);CorsairError rc=CE_Success;
 if(!connected)rc=CE_NotConnected;else if(device<0)rc=CE_DeviceNotFound;
 else{dirty[device]=false;gs_clear(device==0?1:2);}
 ReleaseSRWLockExclusive(&guard);return rc;
}
CorsairError CorsairGetLedLuidForKeyName(const CorsairDeviceId id,char key,CorsairLedLuid* led)
{
 if(!connected)return CE_NotConnected;if(device_index(id)<0)return CE_DeviceNotFound;if(device_index(id)!=0)return CE_InvalidArguments;if(!led)return CE_InvalidArguments;
 const char* names="1234567890QWERTYUIOPASDFGHJKLZXCVBNM";
 const unsigned ids[]={CLK_1,CLK_2,CLK_3,CLK_4,CLK_5,CLK_6,CLK_7,CLK_8,CLK_9,CLK_0,CLK_Q,CLK_W,CLK_E,CLK_R,CLK_T,CLK_Y,CLK_U,CLK_I,CLK_O,CLK_P,CLK_A,CLK_S,CLK_D,CLK_F,CLK_G,CLK_H,CLK_J,CLK_K,CLK_L,CLK_Z,CLK_X,CLK_C,CLK_V,CLK_B,CLK_N,CLK_M};
 if(key>='a'&&key<='z')key-=32;const char* p=strchr(names,key);if(!p||!key){*led=0;return CE_InvalidArguments;}*led=ids[p-names];return CE_Success;
}
// Input interception and hardware properties are deliberately not fabricated.
CorsairError CorsairSubscribeForEvents(CorsairEventHandler,void*){return CE_IncompatibleProtocol;}
CorsairError CorsairUnsubscribeFromEvents(){return CE_IncompatibleProtocol;}
CorsairError CorsairConfigureKeyEvent(const CorsairDeviceId,const CorsairKeyEventConfiguration*){return CE_NotAllowed;}
CorsairError CorsairGetDevicePropertyInfo(const CorsairDeviceId,CorsairDevicePropertyId,unsigned,CorsairDataType*,unsigned*){return CE_IncompatibleProtocol;}
CorsairError CorsairReadDeviceProperty(const CorsairDeviceId,CorsairDevicePropertyId,unsigned,CorsairProperty*){return CE_IncompatibleProtocol;}
CorsairError CorsairWriteDeviceProperty(const CorsairDeviceId,CorsairDevicePropertyId,unsigned,const CorsairProperty*){return CE_IncompatibleProtocol;}
CorsairError CorsairFreeProperty(CorsairProperty*){return CE_IncompatibleProtocol;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID){return TRUE;}
