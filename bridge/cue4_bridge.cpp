#define ICUESDK_EXPORTS
#include "sdk4/iCUESDK.h"
#include "sdk4/iCUESDKLedIdEnum.h"
#include "transport.h"
#include "keyboard4_map.h"
#include <string.h>
static SRWLOCK guard=SRWLOCK_INIT;
static int connected;
static uint32_t colors[126];
static const char device_id[]="OpenRGB.GameSync.Keyboard";
static CorsairSessionStateChangedHandler session_callback;
static void* session_context;
static CorsairSessionDetails details={{4,0,84},{4,0,84},{5,0,0}};
static bool known(const char* id){return id && !strcmp(id,device_id);}
static int index_for(unsigned id){for(int i=0;i<126;++i)if(id && cue4_keys[i]==id)return i;return -1;}
static int led_count(){int n=0;for(int i=0;i<126;++i)n+=cue4_keys[i]!=0;return n;}
struct Callback { CorsairAsyncCallback callback;void* context;CorsairError error; };
static DWORD WINAPI async_result(void* value){auto* d=(Callback*)value;d->callback(d->context,d->error);HeapFree(GetProcessHeap(),0,d);return 0;}
static CorsairError buffer(const char* id,int size,const CorsairLedColor* data)
{
 if(!connected)return CE_NotConnected;
 if(!known(id))return CE_DeviceNotFound;
 if(size<0||size>512||(size&&!data))return CE_InvalidArguments;
 for(int i=0;i<size;++i)if(index_for(data[i].id)<0)return CE_InvalidArguments;
 for(int i=0;i<size;++i){auto c=data[i];int ix=index_for(c.id);uint32_t previous=colors[ix];
  unsigned r=((previous&255)*(255-c.a)+c.r*c.a)/255;
  unsigned g=(((previous>>8)&255)*(255-c.a)+c.g*c.a)/255;
  unsigned b=(((previous>>16)&255)*(255-c.a)+c.b*c.a)/255;
  colors[ix]=r|(g<<8)|(b<<16);
 }
 return CE_Success;
}
CorsairError CorsairConnect(CorsairSessionStateChangedHandler callback,void* context)
{
 if(!callback)return CE_InvalidArguments;
 AcquireSRWLockExclusive(&guard);
 if(connected){ReleaseSRWLockExclusive(&guard);return CE_InvalidOperation;}
 connected=gs_init(2,(const void*)(uintptr_t)&CorsairConnect);
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
 if(!connected)return CE_NotConnected;if(!info)return CE_InvalidArguments;if(!known(id))return CE_DeviceNotFound;
 memset(info,0,sizeof(*info));info->type=CDT_Keyboard;strcpy(info->id,device_id);strcpy(info->model,"OpenRGB virtual keyboard");info->ledCount=led_count();return CE_Success;
}
CorsairError CorsairGetDevices(const CorsairDeviceFilter* filter,int max,CorsairDeviceInfo* devices,int* size)
{
 if(!connected)return CE_NotConnected;if(!filter||!size||max<0||(max&&!devices))return CE_InvalidArguments;
 *size=(max && (filter->deviceTypeMask&CDT_Keyboard))?1:0;
 if(*size)return CorsairGetDeviceInfo(device_id,devices);return CE_Success;
}
CorsairError CorsairGetLedPositions(const CorsairDeviceId id,int max,CorsairLedPosition* positions,int* size)
{
 if(!connected)return CE_NotConnected;if(!known(id))return CE_DeviceNotFound;
 if(!size||max<0||(max&&!positions))return CE_InvalidArguments;*size=0;
 for(int i=0;i<126&&*size<max;++i)if(cue4_keys[i]){positions[*size]={cue4_keys[i],double(i%21)*19,double(i/21)*19};++*size;}
 return CE_Success;
}
CorsairError CorsairSetLedColorsBuffer(const CorsairDeviceId id,int size,const CorsairLedColor* data)
{AcquireSRWLockExclusive(&guard);auto rc=buffer(id,size,data);ReleaseSRWLockExclusive(&guard);return rc;}
CorsairError CorsairSetLedColors(const CorsairDeviceId id,int size,const CorsairLedColor* data)
{AcquireSRWLockExclusive(&guard);auto rc=buffer(id,size,data);if(rc==CE_Success&&!gs_frame(1,6,21,colors))rc=CE_NotConnected;ReleaseSRWLockExclusive(&guard);return rc;}
CorsairError CorsairSetLedColorsFlushBufferAsync(CorsairAsyncCallback callback,void* context)
{
 if(!callback)return CE_InvalidArguments;
 AcquireSRWLockExclusive(&guard);CorsairError rc=connected&&gs_frame(1,6,21,colors)?CE_Success:CE_NotConnected;ReleaseSRWLockExclusive(&guard);
 if(!callback)return CE_InvalidArguments;
 auto* data=(Callback*)HeapAlloc(GetProcessHeap(),0,sizeof(Callback));if(!data)return CE_InvalidOperation;
 *data={callback,context,rc};if(!QueueUserWorkItem(async_result,data,WT_EXECUTEDEFAULT)){HeapFree(GetProcessHeap(),0,data);return CE_InvalidOperation;}return rc;
}
CorsairError CorsairGetLedColors(const CorsairDeviceId id,int size,CorsairLedColor* data)
{
 AcquireSRWLockShared(&guard);CorsairError rc=CE_Success;
 if(!connected)rc=CE_NotConnected;else if(!known(id))rc=CE_DeviceNotFound;
 else if(size<0||size>512||(size&&!data))rc=CE_InvalidArguments;
 else{
  for(int i=0;i<size;++i)if(index_for(data[i].id)<0)rc=CE_InvalidArguments;
  if(rc==CE_Success)for(int i=0;i<size;++i){auto c=colors[index_for(data[i].id)];data[i].r=c&255;data[i].g=(c>>8)&255;data[i].b=(c>>16)&255;data[i].a=255;}
 }
 ReleaseSRWLockShared(&guard);return rc;
}
CorsairError CorsairSetLayerPriority(unsigned priority){return !connected?CE_NotConnected:priority>255?CE_InvalidArguments:CE_Success;}
CorsairError CorsairRequestControl(const CorsairDeviceId id,CorsairAccessLevel level){return !connected?CE_NotConnected:!known(id)?CE_DeviceNotFound:level==CAL_Shared||level==CAL_ExclusiveLightingControl?CE_Success:CE_NotAllowed;}
CorsairError CorsairReleaseControl(const CorsairDeviceId id){if(!connected)return CE_NotConnected;if(!known(id))return CE_DeviceNotFound;gs_suspend();return CE_Success;}
CorsairError CorsairGetLedLuidForKeyName(const CorsairDeviceId id,char key,CorsairLedLuid* led)
{
 if(!connected)return CE_NotConnected;if(!known(id))return CE_DeviceNotFound;if(!led)return CE_InvalidArguments;
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
