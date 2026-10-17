#include <cassert>
#include "../bridge/cue_bridge.cpp"
static int emissions;
static uint32_t first;
extern "C" int gs_init(unsigned source,const void*){assert(source==2);return 1;}
extern "C" int gs_frame(unsigned role,unsigned,unsigned,const uint32_t* data){assert(role==1);first=data[0];++emissions;return 1;}
extern "C" void gs_shutdown(){}
extern "C" void gs_suspend(){}
extern "C" void gs_clear(unsigned){}
int main()
{
    assert(CorsairPerformProtocolHandshake().serverProtocolVersion==16);
    CorsairLedColor mixed[]={{CLK_Escape,10,20,30},{CLK_G1,1,2,3},{CLK_Mute,4,5,6}};
    assert(CorsairSetLedsColorsBufferByDeviceIndex(0,3,mixed));
    assert(CorsairSetLedsColorsFlushBuffer()&&emissions==1&&first==0x1e140a);
    assert(CorsairGetLedsColorsByDeviceIndex(0,3,mixed));
    CorsairLedColor absent{CLK_G1,10,20,30};
    assert(CorsairSetLedsColorsBufferByDeviceIndex(0,1,&absent));
    assert(CorsairSetLedsColorsFlushBuffer()&&emissions==1);
    CorsairLedColor duplicate[]={{CLK_Escape,255,0,0},{CLK_Escape,0,255,0}};
    assert(!CorsairSetLedsColorsBufferByDeviceIndex(0,2,duplicate));
    assert(CorsairGetLastError()==CE_InvalidArguments);
    CorsairLedColor readback{CLK_Escape,0,0,0};
    assert(CorsairGetLedsColorsByDeviceIndex(0,1,&readback)&&readback.r==10);
    mixed[1].g=256;
    assert(!CorsairSetLedsColorsBufferByDeviceIndex(0,3,mixed));
    assert(CorsairReleaseControl(CAM_ExclusiveLightingControl));
}
