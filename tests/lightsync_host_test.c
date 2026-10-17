#include <assert.h>
#include <string.h>
#include "../bridge/lightsync_bridge.c"
ULONGLONG test_tick;
static uint32_t output[126],background;
static int emissions;
int gs_init(unsigned source,const void* module){(void)module;assert(source==3);return 1;}
int gs_frame(unsigned role,unsigned rows,unsigned cols,const uint32_t* data){
    (void)rows;if(role==1)memcpy(output,data,cols*rows*sizeof(uint32_t));else if(role==6)background=*data;
    ++emissions;return 1;
}
void gs_suspend(void){}
void gs_clear(unsigned role){(void)role;}
void gs_shutdown(void){}
int main(void)
{
    assert(!LogiLedFlashLighting(100,0,0,200,100));
    assert(LogiLedInit());
    assert(LogiLedStopEffects()&&LogiLedStopEffectsOnKey(1)&&emissions==0);
    assert(LogiLedSetLighting(0,0,100));
    test_tick=100;
    assert(LogiLedFlashLighting(100,0,0,200,100));
    assert(output[0]==0xff&&background==0xff);
    animation_tick(150);assert(output[0]==0xff0000);
    animation_tick(200);assert(output[0]==0xff);
    animation_tick(300);assert(output[0]==0xff0000&&!global_effect.mode);
    assert(!LogiLedPulseLighting(0,100,0,100,0));
    test_tick=400;
    assert(LogiLedPulseLighting(100,0,0,0,200));
    animation_tick(500);assert(output[0]==0xff);
    assert(LogiLedStopEffects());assert(output[0]==0xff0000);
    test_tick=600;
    assert(LogiLedFlashSingleKey(1,0,100,0,100,40));
    assert(output[0]==0xff00&&output[1]==0xff0000);
    animation_tick(700);assert(output[0]==0xff0000);
    test_tick=800;
    assert(LogiLedPulseSingleKey(1,100,0,0,0,100,0,200,TRUE));
    animation_tick(900);assert(output[0]==0xff00);
    assert(LogiLedStopEffectsOnKey(1));assert(output[0]==0xff0000);
    test_tick=1000;
    assert(LogiLedFlashLighting(100,0,0,0,100));
    assert(LogiLedSetLightingForKeyWithScanCode(1,0,100,0));
    animation_tick(1100);assert(output[0]==0xff00&&output[1]==0xff);
    assert(LogiLedRestoreLighting());
    int previous=emissions;animation_tick(1200);assert(emissions==previous);
    test_tick=0xffffffffu-20;
    assert(LogiLedSetLighting(0,0,100));
    assert(LogiLedFlashLighting(100,0,0,100,100));
    animation_tick(40);assert(output[0]==0xff0000);
    animation_tick(120);assert(!global_effect.mode&&output[0]==0xff0000);
    test_tick=140;
    assert(LogiLedSetTargetDevice(4)&&LogiLedFlashLighting(100,0,0,0,100));
    assert(output[0]==0xff);
    assert(LogiLedSetTargetDevice(1)&&LogiLedSetLighting(0,100,0));
    assert(output[0]==0xff0000&&background==0xff00);
    LogiLedShutdown();assert(!LogiLedFlashSingleKey(1,100,0,0,0,100));
}
