#include <cassert>
#include <cstring>
#include <vector>
#include "../bridge/cue4_bridge.cpp"
struct Frame {unsigned role,rows,cols;std::vector<uint32_t> colors;};
static std::vector<Frame> frames;
static unsigned cleared;
extern "C" int gs_init(unsigned source,const void*){assert(source==2);return 1;}
extern "C" int gs_frame(unsigned role,unsigned rows,unsigned cols,const uint32_t* data){frames.push_back({role,rows,cols,{data,data+rows*cols}});return 1;}
extern "C" void gs_shutdown(){}
extern "C" void gs_suspend(){}
extern "C" void gs_clear(unsigned role){cleared=role;}
static bool notified,completed;
static void state(void*,const CorsairSessionStateChanged* event){notified=event->state==CSS_Connected;}
static void done(void*,CorsairError error){assert(error==CE_Success);completed=true;}
int main()
{
 assert(CorsairConnect(state,nullptr)==CE_Success&&notified);
 CorsairDeviceFilter filter{static_cast<int>(CDT_All)};CorsairDeviceInfo devices[2];int count=-1;
 assert(CorsairGetDevices(&filter,2,devices,&count)==CE_Success&&count==2);
 assert(devices[0].type==CDT_Keyboard&&devices[1].type==CDT_Mouse);
 filter.deviceTypeMask=CDT_Mouse;
 CorsairDeviceInfo mouse;
 assert(CorsairGetDevices(&filter,1,&mouse,&count)==CE_Success&&count==1&&mouse.type==CDT_Mouse);
 CorsairLedPosition positions[4];
 assert(CorsairGetLedPositions(mouse.id,4,positions,&count)==CE_Success&&count==4);
 assert(positions[0].id==(4u<<16|1u));
 // Both device buffers flush once, without confusing mouse LEDs with keyboard LEDs.
 CorsairLedColor key{CLK_Escape,1,2,3,255},pixel{positions[0].id,10,20,30,255};
 assert(CorsairSetLedColorsBuffer(devices[0].id,1,&key)==CE_Success);
 assert(CorsairSetLedColorsBuffer(mouse.id,1,&pixel)==CE_Success);
 assert(frames.empty());
 assert(CorsairSetLedColorsFlushBufferAsync(done,nullptr)==CE_Success&&completed);
 assert(frames.size()==2&&frames[0].role==1&&frames[1].role==2);
 assert(frames[1].cols==4&&frames[1].colors[0]==0x1e140a);
 completed=false;
 assert(CorsairSetLedColorsFlushBufferAsync(done,nullptr)==CE_Success&&completed&&frames.size()==2);
 pixel.r=0;
 assert(CorsairGetLedColors(mouse.id,1,&pixel)==CE_Success&&pixel.r==10);
 CorsairLedColor invalid{CLK_Escape,255,0,0,255};
 assert(CorsairSetLedColors(mouse.id,1,&invalid)==CE_InvalidArguments);
 assert(CorsairGetLedColors(mouse.id,1,&pixel)==CE_Success&&pixel.r==10);
 assert(CorsairReleaseControl(mouse.id)==CE_Success&&cleared==2);
 assert(CorsairGetDeviceInfo("unknown",&mouse)==CE_DeviceNotFound);
 assert(CorsairDisconnect()==CE_Success);
 assert(CorsairGetDevices(&filter,2,devices,&count)==CE_NotConnected);
}
