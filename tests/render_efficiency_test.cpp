#include <QApplication>
#include <QCheckBox>
#include <QThread>
#include <QUuid>
#include <cassert>
#include <iostream>
#include "NullInterfaces.h"
#include "../GameSyncPlugin.h"

class Device : public NullController
{
public:
    RGBColor color = 0;
    unsigned writes = 0;
    unsigned sets = 0;
    int mode = 0;
    unsigned int GetLEDCount() override { return 1; }
    unsigned int GetModeCount() override { return 1; }
    int GetActiveMode() override { return mode; }
    void SetActiveMode(int value) override { mode=value; }
    std::string GetModeName(unsigned int) override { return "Direct"; }
    RGBColor GetColor(unsigned int) override { return color; }
    void SetAllColors(RGBColor value) override { color = value;++sets; }
    void SetColor(unsigned int, RGBColor value) override { color = value;++sets; }
    void UpdateLEDs() override { ++writes; }
};
class API : public NullAPI
{
public:
    Device device;
    std::vector<RGBControllerInterface*> GetRGBControllers() override { return {&device}; }
};
struct GameReceiverTest
{
    static void inject(GameReceiver& receiver,const GameSyncPacket& p)
    {receiver.processPacket(QByteArray(reinterpret_cast<const char*>(&p),GS_HEADER_SIZE+p.count*4));}
};
struct GameSyncPluginTest
{
    static void initialize(GameSyncPlugin& plugin,API& api)
    {
        plugin.api=&api;
        plugin.widget=new QWidget;
        plugin.toggle=new QCheckBox(plugin.widget);plugin.toggle->setChecked(true);
        plugin.receiver=new GameReceiver(&plugin);
        plugin.receiver->changed=[&plugin]{plugin.dirty=true;};
        plugin.receiver->released=[&plugin]{plugin.relinquish();};
    }
    static void frame(GameSyncPlugin& plugin,const GameSyncPacket& p)
    {GameReceiverTest::inject(*plugin.receiver,p);plugin.render();}
    static quint64 writes(const GameSyncPlugin& plugin){return plugin.hardwareWrites;}
    static void heartbeat(GameSyncPlugin& plugin){plugin.render();}
    static void invalidate(GameSyncPlugin& plugin){plugin.dirty=true;}
    static void failure(GameSyncPlugin& plugin,unsigned source){plugin.receiver->releaseSource(source);}
};
int main(int argc,char** argv)
{
    QApplication app(argc,argv);
    qputenv("OPENRGB_GAME_SYNC_LEASE_KEY",QUuid::createUuid().toByteArray());
    API api;api.device.color=ToRGBColor(32,16,8);
    GameSyncPlugin plugin;GameSyncPluginTest::initialize(plugin,api);
    GameSyncPacket p{};p.magic=GS_MAGIC;p.version=2;p.kind=1;p.device=0x201;
    p.rows=1;p.cols=2;p.count=2;p.session[0]=1;
    p.colors[0]=ToRGBColor(255,0,0);p.colors[1]=ToRGBColor(0,0,255);
    GameSyncPluginTest::frame(plugin,p);
    assert(api.device.writes==1&&api.device.sets==1&&api.device.color==ToRGBColor(127,0,127));
    for(int i=0;i<1000;++i)GameSyncPluginTest::frame(plugin,p);
    assert(api.device.writes==1&&api.device.sets==1&&GameSyncPluginTest::writes(plugin)==1);
    std::swap(p.colors[0],p.colors[1]); // Different packet, identical physical output.
    GameSyncPluginTest::frame(plugin,p);assert(api.device.writes==1&&api.device.sets==1);
    p.colors[0]=p.colors[1]=ToRGBColor(0,255,0);
    GameSyncPluginTest::frame(plugin,p);assert(api.device.writes==2&&api.device.color==ToRGBColor(0,255,0));
    // No changed colors still needs a renewed lease, or Effects would resume.
    QElapsedTimer elapsed;elapsed.start();
    while(elapsed.elapsed()<2100){GameSyncPluginTest::heartbeat(plugin);QThread::msleep(10);}
    {PriorityLease lease;PriorityLock lock(lease);assert(lock.acquired&&lease.active());}
    assert(api.device.writes==2);
    api.device.mode=1;GameSyncPluginTest::invalidate(plugin);GameSyncPluginTest::heartbeat(plugin);
    assert(api.device.mode==0&&api.device.writes==3&&api.device.sets==3);
    GameSyncPluginTest::failure(plugin,1);assert(api.device.color==ToRGBColor(0,255,0));
    GameSyncPluginTest::failure(plugin,2);assert(api.device.color==ToRGBColor(32,16,8));
    {PriorityLease lease;PriorityLock lock(lease);assert(lock.acquired&&!lease.active());}
    // A later identical frame must repaint after restoring the old effect.
    const auto restored=api.device.writes;
    GameSyncPluginTest::frame(plugin,p);assert(api.device.writes==restored+1);
    plugin.Unload();
    std::cout<<"PASS: 1000 duplicate packets produce no extra writes; equal device output is skipped; color changes repaint\n";
    std::cout<<"PASS: idle render renews game priority; helper loss restores the old effect; reacquisition repaints\n";
}
