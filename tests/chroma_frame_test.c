#include <assert.h>
#include <string.h>
#include "../bridge/chroma_frames.h"
int main(void)
{
    GameSyncPacket frame = {0};
    uint32_t input[192+132] = {0};
    for (int i=0;i<192;++i) input[i]=0x123456;
    input[192]=0x010000ff;input[192+131]=0x0100ff00;
    assert(chroma_frame(1,9,input,&frame)==0);
    assert(frame.rows==8&&frame.cols==24&&frame.count==192);
    assert(frame.colors[0]==0xff&&frame.colors[191]==0xff00);
    assert(frame.colors[24+10]==0x123456);
    uint32_t legacy[264]={0};legacy[0]=0x11;legacy[132]=0x1000022;
    assert(chroma_frame(1,8,legacy,&frame)==0&&frame.count==132&&frame.colors[0]==0x22);
    uint32_t mouse[]={0xabcdef};
    assert(chroma_frame(2,9,mouse,&frame)==0&&frame.colors[29]==0xabcdef);
    assert(chroma_frame(4,6,input,&frame)==0&&frame.count==20);
    assert(chroma_frame(1,0,0,&frame)==0&&frame.colors[0]==0);
    assert(chroma_frame(1,9,0,&frame)==87);
    assert(chroma_frame(1,6,input,&frame)==50);
    assert(chroma_frame(99,0,0,&frame)==87);
    assert(sizeof(GameSyncPacket)==96+192*4);
}
