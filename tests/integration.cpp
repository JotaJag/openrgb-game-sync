#include <QApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QThread>
#include <QUdpSocket>
#include <QUuid>
#include <atomic>
#include <iostream>
#include "NullInterfaces.h"
#include "GameSyncPlugin.h"
#include "Effects/Sequence/Sequence.h"
#include "EffectManager.h"
#include "OpenRGBEffectsPlugin.h"

class Device : public NullController
{
public:
    RGBColor color = 0;
    unsigned int GetLEDCount() override { return 1; }
    unsigned int GetZoneCount() override { return 1; }
    unsigned int GetZoneLEDsCount(unsigned int) override { return 1; }
    unsigned int GetModeCount() override { return 1; }
    RGBColor GetColor(unsigned int) override { return color; }
    void SetColor(unsigned int, RGBColor c) override { color = c; }
    std::string GetModeName(unsigned int) override { return "Direct"; }
    void SetAllColors(RGBColor c) override { color = c; }
    void SetAllZoneColors(int, RGBColor c) override { color = c; }
};

class API : public NullAPI
{
public:
    std::string configDirectory;
    filesystem::path GetConfigurationDirectory() override { return configDirectory; }
    Device device;
    Device staticDevice;
    nlohmann::json settings = nullptr; // First launch has no settings yet.
    std::vector<RGBControllerInterface*> GetRGBControllers() override { return {&device, &staticDevice}; }
    nlohmann::json GetSettings(std::string) override { return settings; }
    void SetSettings(std::string, nlohmann::json value) override { settings = value; }
};

class CountedSequence : public Sequence
{
public:
    std::atomic<unsigned int> steps{0};
    void StepEffect(std::vector<ControllerZone*> zones) override
    {
        Sequence::StepEffect(zones);
        ++steps;
    }
};

static void wait(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms) { QApplication::processEvents(); QThread::msleep(2); }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    const QString sourceRoot = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    QUdpSocket probe;
    if (!probe.bind(QHostAddress(QHostAddress::LocalHost), quint16(0))) return 2;
    quint16 port = probe.localPort(); probe.close();
    qputenv("OPENRGB_GAME_SYNC_PORT", QByteArray::number(port));
    qputenv("OPENRGB_GAME_SYNC_LEASE_KEY", QUuid::createUuid().toByteArray());
    API api;
    api.staticDevice.color = ToRGBColor(32,16,8);
    OpenRGBEffectsPlugin::api = &api;
    OpenRGBEffectsPlugin::controllers_updating = false;
    GameSyncPlugin plugin;
    plugin.Load(&api);
    auto* toggle = plugin.GetWidget()->findChild<QCheckBox*>("gamePriority");
    if (!toggle || !toggle->isChecked()) return 2;
    CountedSequence effect;
    effect.SetFPS(60); effect.SetSpeed(4); effect.SetSlider2Val(1);
    effect.SetBrightness(100); effect.SetTemperature(0); effect.SetTint(0);
    effect.LoadCustomSettings({{"colors", {ToRGBColor(255,0,0), ToRGBColor(0,0,255)}}});
    ControllerZone zone(&api.device, 0, false, 100, true, false);
    auto* manager = EffectManager::Get();
    manager->Assign({&zone}, &effect);
    manager->SetEffectActive(&effect);
    bool ok = true;
    auto check = [&ok](bool value, const char* description) {
        std::cout << (value ? "PASS: " : "FAIL: ") << description << '\n'; ok &= value;
    };
    wait(100);
    check(effect.steps > 2, "ordinary Sequence runs when no compatible game is present");
    QUdpSocket sender;
    GameSyncPacket packet{};
    packet.magic = GS_MAGIC; packet.version = 1; packet.kind = 1;
    packet.device = 6; packet.rows = 1; packet.cols = 1; packet.count = 1;
    packet.session[0] = 42; std::memcpy(packet.app, "Integration test", 16);
    packet.colors[0] = ToRGBColor(0,255,0);
    auto send = [&] { sender.writeDatagram(reinterpret_cast<char*>(&packet), GS_HEADER_SIZE + std::min<unsigned>(packet.count, GS_MAX_COLORS) * 4, QHostAddress::LocalHost, port); };
    packet.count = 133; send(); wait(100);
    auto before = effect.steps.load(); wait(100);
    check(effect.steps > before, "invalid input cannot suppress the ordinary effect");
    packet.count = 1; send(); wait(150);
    before = effect.steps.load(); wait(100);
    check(effect.steps == before, "compatible frames pause Sequence progression and hardware writes");
    {
        PriorityLease lease; PriorityLock lock(lease);
        check(api.device.color == ToRGBColor(0,255,0), "game color reaches the OpenRGB controller without competing writes");
    }
    toggle->setChecked(false); wait(100);
    check(effect.steps > before && api.settings["enabled"] == false, "switch disables game priority and immediately restores Sequence");
    check(api.staticDevice.color == ToRGBColor(32,16,8), "devices outside Sequence recover their previous static lighting");
    before = effect.steps.load(); send(); wait(100);
    check(effect.steps > before, "game packets are ignored while integration is disabled");
    toggle->setChecked(true);
    for (unsigned source : {2u, 3u})
    {
        auto* sourceToggle = plugin.GetWidget()->findChild<QCheckBox*>("source" + QString::number(source));
        packet.version = 2; packet.device = (source << 8) | 6;
        sourceToggle->setChecked(false); before = effect.steps.load(); send(); wait(100);
        check(effect.steps > before, source == 2 ? "disabled iCUE cannot take priority" : "disabled Lightsync cannot take priority");
        sourceToggle->setChecked(true); send(); wait(100); before = effect.steps.load(); wait(100);
        check(effect.steps == before, source == 2 ? "iCUE frames take exclusive priority" : "Lightsync frames take exclusive priority");
        sourceToggle->setChecked(false); wait(100);
        check(effect.steps > before, source == 2 ? "disabling iCUE restores Sequence" : "disabling Lightsync restores Sequence");
    }
    packet.version = 1; packet.device = 6;
    toggle->setChecked(true); send(); wait(100);
    before = effect.steps.load(); wait(1700);
    check(effect.steps > before, "loss of game heartbeat restores the previous effect automatically");
    QProcess restClient;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("OPENRGB_GAME_SYNC_PORT", QString::number(port));
    restClient.setProcessEnvironment(environment);
    restClient.setWorkingDirectory((sourceRoot + ""));
    restClient.start("/usr/bin/python3", {"-c", R"PY(
from chroma_rest import Server
import threading,http.client,json,time
server=Server(('127.0.0.1',0),int(__import__('os').environ['OPENRGB_GAME_SYNC_PORT']))
threading.Thread(target=server.serve_forever,daemon=True).start()
def request(method,path,body):
 conn=http.client.HTTPConnection('127.0.0.1',server.server_port)
 conn.request(method,path,json.dumps(body)); response=conn.getresponse(); data=json.loads(response.read()); conn.close(); return data
session=request('POST','/razer/chromasdk',{'title':'REST integration'})
path='/'+session['uri'].split('/',3)[3]
request('PUT',path+'/chromalink',{'effect':'CHROMA_STATIC','param':{'color':65280}})
time.sleep(.5)
request('DELETE',path,{})
server.shutdown();server.server_close()
)PY"});
    wait(300); before = effect.steps.load(); wait(100);
    check(api.device.color == ToRGBColor(0,255,0) && effect.steps == before,
          "actual Chroma REST HTTP request reaches the renderer and pauses Sequence");
    wait(1200);
    check(effect.steps > before && restClient.state() == QProcess::NotRunning && restClient.exitCode() == 0,
          "closing a Chroma REST session restores Sequence");
    send(); wait(100); before = effect.steps.load();
    plugin.Unload(); wait(100);
    check(effect.steps > before, "unloading the plugin releases priority");
    manager->SetEffectUnActive(&effect);
    manager->RemoveMapping(&effect);
    QTemporaryDir testHome;
    auto oldHome=qgetenv("HOME"); qputenv("HOME",testHome.path().toUtf8());
    QTcpServer portProbe; portProbe.listen(QHostAddress::LocalHost,0);
    auto restPort=portProbe.serverPort(); portProbe.close();
    qputenv("OPENRGB_GAME_SYNC_REST_PORT",QByteArray::number(restPort));
    api.configDirectory=(testHome.path()+"/.config/OpenRGB").toStdString();
    auto configPath=QString::fromStdString(api.configDirectory)+"/plugins/settings";
    QDir().mkpath(configPath);
    QFile config(configPath+"/GameSyncAutomation.json"); if (!config.open(QIODevice::WriteOnly)) return 2;
    QFile prefixFile((sourceRoot + "/build/pipe-lab/prefix.txt"));
    if (!prefixFile.open(QIODevice::ReadOnly)) return 2;
    auto sdkPrefix=QString::fromUtf8(prefixFile.readAll()).trimmed();
    auto sdkStatus=testHome.path()+"/pipe-status.json";
    config.write(QJsonDocument(QJsonObject{{"enabled",true},{"rest",true},
        {"pipe_services",QJsonArray{QJsonObject{{"prefix",sdkPrefix},{"wine","/usr/bin/wine"},{"status",sdkStatus}}}},
        {"script",(sourceRoot + "/auto_prepare.py")},
        {"status",testHome.path()+"/.local/share/openrgb-game-sync/automation/status.json"}}).toJson()); config.close();
    qputenv("OPENRGB_GAME_SYNC_BACKGROUND","1");
    plugin.Load(&api);
    plugin.GetWidget()->findChild<QCheckBox*>("source2")->setChecked(true);
    wait(500);
    QTcpSocket http; http.connectToHost(QHostAddress::LocalHost,restPort);
    bool connected=http.waitForConnected(1000);
    QByteArray response;
    if(connected){http.write("GET /razer/chromasdk HTTP/1.0\r\nHost: localhost\r\n\r\n"); http.waitForBytesWritten(500); if(http.waitForReadyRead(1000)) response=http.readAll();}
    check(connected && response.contains("200 OK"),"loading the plugin starts its isolated Chroma REST helper");
    QElapsedTimer sdkReady; sdkReady.start();
    while(sdkReady.elapsed()<8000) {
        QFile stateFile(sdkStatus);
        if(stateFile.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(stateFile.readAll()).object().value("running").toBool()) break;
        wait(50);
    }
    QProcess sdkClient;
    sdkClient.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    auto sdkEnv=sdkClient.processEnvironment(); sdkEnv.insert("WINEPREFIX",sdkPrefix);sdkEnv.insert("WINEDEBUG","-all");sdkClient.setProcessEnvironment(sdkEnv);
    sdkClient.setWorkingDirectory((sourceRoot + "/build/pipe-lab"));
    sdkClient.start("/usr/bin/wine", {(sourceRoot + "/build/pipe-lab/original-probe.exe")});
    QElapsedTimer colorWait; colorWait.start();
    while(colorWait.elapsed()<8000 && api.device.color != ToRGBColor(10,20,30)) wait(10);
    check(api.device.color==ToRGBColor(10,20,30),"unchanged original Corsair DLL reaches the renderer through the plugin-managed pipe service");
    QElapsedTimer sdkFinished;sdkFinished.start();
    while(sdkClient.state()!=QProcess::NotRunning && sdkFinished.elapsed()<4000) wait(25);
    check(sdkClient.state()==QProcess::NotRunning && sdkClient.exitCode()==0,"original DLL API checks pass under the plugin-managed service");
    auto* lifecycleToggle=plugin.GetWidget()->findChild<QCheckBox*>("gamePriority");
    lifecycleToggle->setChecked(false); wait(200);
    QTcpSocket stopped; stopped.connectToHost(QHostAddress::LocalHost,restPort);
    check(!stopped.waitForConnected(300),"disabling Game Sync stops the REST helper");
    QFile stoppedState(sdkStatus);bool pipeStopped=false;
    if(stoppedState.open(QIODevice::ReadOnly))pipeStopped=!QJsonDocument::fromJson(stoppedState.readAll()).object().value("running").toBool();
    check(pipeStopped,"disabling Game Sync stops the original-DLL service");
    plugin.Unload(); qputenv("HOME",oldHome); qunsetenv("OPENRGB_GAME_SYNC_REST_PORT"); qunsetenv("OPENRGB_GAME_SYNC_BACKGROUND");
    return ok ? 0 : 1;
}
