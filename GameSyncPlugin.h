#pragma once
#include <QCheckBox>
#include <QLabel>
#include <QTimer>
#include <QProcess>
#include <atomic>
#include <mutex>
#include "OpenRGBPluginInterface.h"
#include "GameReceiver.h"
#include "PriorityLease.h"
#include "HelperProcess.h"

class GameSyncPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "plugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)
    OpenRGBPluginAPIInterface* api = nullptr;
    QWidget* widget = nullptr;
    QCheckBox* toggle = nullptr;
    QCheckBox* sourceToggles[3] = {};
    QLabel* label = nullptr;
    QLabel* automationLabel = nullptr;
    QLabel* pipeLabel = nullptr;
    QLabel* diagnosticsLabel = nullptr;
    GameReceiver* receiver = nullptr;
    QTimer* renderTimer = nullptr;
    QTimer* automationTimer = nullptr;
    QProcess* automationProcess = nullptr;
    HelperProcess* restProcess = nullptr;
    std::vector<HelperProcess*> pipeProcesses;
    QStringList pipeStatusPaths;
    QTimer* pipeStatusTimer = nullptr;
    void managePipeServices(bool stopping = false);
    void updatePipeStatus();
    void updateDiagnostics();
    QTimer* automationDeadline = nullptr;
    QString automationStatusPath;
    void updateAutomationStatus();
    void manageRest();
    void prepareGames();
    PriorityLease lease{true};
    std::mutex devicesMutex;
    struct Snapshot { int mode; std::vector<RGBColor> colors, submitted; };
    std::map<RGBControllerInterface*, Snapshot> snapshots;
    bool detecting = false, dirty = false, ready = false;
    quint64 hardwareWrites = 0;
    void render();
    void relinquish();
    void saveSettings();
    friend struct GameSyncPluginTest;
public:
    OpenRGBPluginInfo GetPluginInfo() override;
    unsigned int GetPluginAPIVersion() override { return OPENRGB_PLUGIN_API_VERSION; }
    void Load(OpenRGBPluginAPIInterface* value) override;
    QWidget* GetWidget() override { return widget; }
    QMenu* GetTrayMenu() override { return nullptr; }
    void Unload() override;
    void OnProfileAboutToLoad() override;
    void OnProfileLoad(nlohmann::json) override {}
    nlohmann::json OnProfileSave() override { return nlohmann::json::object(); }
    unsigned char* OnSDKCommand(unsigned int, unsigned char*, unsigned int* size) override { *size = 0; return nullptr; }
    void ProfileManagerUpdated(unsigned int) override {}
    void ResourceManagerUpdated(unsigned int) override;
    void SettingsManagerUpdated(unsigned int) override {}
};
