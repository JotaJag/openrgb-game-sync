#include <QCoreApplication>
#include <cassert>
#include <cstring>
#include "../GameReceiver.h"
struct GameReceiverTest
{
    static void inject(GameReceiver& receiver,const GameSyncPacket& packet,int length=-1)
    {receiver.processPacket(QByteArray(reinterpret_cast<const char*>(&packet),length<0?GS_HEADER_SIZE+packet.count*4:length));}
    static void age(GameReceiver& receiver,qint64 ms){receiver.lastFrame=receiver.clock.elapsed()-ms;}
};
int main(int argc,char** argv)
{
    QCoreApplication app(argc,argv);GameReceiver receiver;
    int changes=0;
    receiver.changed=[&]{++changes;};
    GameSyncPacket p{};p.magic=GS_MAGIC;p.version=2;p.kind=1;p.device=0x101;
    p.rows=8;p.cols=24;p.count=192;p.session[0]=1;p.colors[191]=0x123456;
    GameReceiverTest::inject(receiver,p);
    assert(receiver.active()&&receiver.frames[1].colors.size()==192&&receiver.frames[1].colors[191]==0x123456);
    assert(changes==1);
    GameReceiverTest::age(receiver,1000);
    for(int i=0;i<1000;++i)GameReceiverTest::inject(receiver,p);
    assert(changes==1&&receiver.statistics().received==1001&&receiver.statistics().duplicates==1000);
    assert(receiver.statistics().lastFrameAgeMs<100);
    receiver.releaseSource(2);assert(receiver.active());
    // Malformed size and other owners cannot replace the selected frame.
    p.colors[191]=0xffffff;GameReceiverTest::inject(receiver,p,GS_HEADER_SIZE+132*4);
    assert(receiver.frames[1].colors[191]==0x123456);
    p.session[0]=2;GameReceiverTest::inject(receiver,p);
    assert(receiver.frames[1].colors[191]==0x123456);
    p.session[0]=1;p.colors[191]=0x01000000|0x123456;GameReceiverTest::inject(receiver,p);
    assert(changes==1); // The unused alpha byte must not produce hardware updates.
    p.colors[191]=0xabcdef;GameReceiverTest::inject(receiver,p);assert(changes==2);
    receiver.releaseSource(1);assert(!receiver.active());
    receiver.release();p.session[0]=1;p.device=0x201;p.rows=6;p.cols=21;p.count=126;
    GameReceiverTest::inject(receiver,p);
    p.device=0x202;p.rows=1;p.cols=4;p.count=4;GameReceiverTest::inject(receiver,p);
    assert(receiver.frames.size()==2);
    p.kind=3;p.rows=p.cols=p.count=0;
    p.device=0x302;GameReceiverTest::inject(receiver,p);assert(receiver.frames.size()==2);
    p.device=0x202;GameReceiverTest::inject(receiver,p);
    assert(receiver.active()&&receiver.frames.size()==1&&receiver.frames.contains(1));
    p.device=0x201;GameReceiverTest::inject(receiver,p);assert(!receiver.active()&&receiver.frames.isEmpty());
    // Old v1/132-color senders continue to work with the expanded receiver.
    p.version=1;p.kind=1;p.device=1;p.rows=6;p.cols=22;p.count=132;
    GameReceiverTest::inject(receiver,p);assert(receiver.active());
    receiver.setSourceEnabled(1,false);assert(!receiver.active());
    GameReceiverTest::inject(receiver,p);assert(!receiver.active());
}
