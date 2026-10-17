#include "GameSyncPlugin.h"
#include "ResourceManagerCallback.h"
#include <QVBoxLayout>
#include <QMetaObject>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

OpenRGBPluginInfo GameSyncPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info{};
    info.Name = "OpenRGB Game Sync";
    info.Description = "Iluminación Chroma, iCUE y Lightsync con prioridad automática";
    info.Version = "0.6.0-alpha.1";
    info.Commit = "local";
    info.Label = "Game Sync";
    info.Location = OPENRGB_PLUGIN_LOCATION_TOP;
    info.ProtocolVersion = 0;
    return info;
}

void GameSyncPlugin::Load(OpenRGBPluginAPIInterface* value)
{
    api = value;
    widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    toggle = new QCheckBox("Dar prioridad a la iluminación de juegos compatibles", widget);
    toggle->setObjectName("gamePriority");
    label = new QLabel(widget);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    layout->addWidget(toggle);
    const QString sourceNames[] = {"Razer Chroma", "Corsair iCUE (SDK 3 y 4)", "Logitech Lightsync"};
    for (unsigned i = 0; i < 3; ++i)
    {
        sourceToggles[i] = new QCheckBox(sourceNames[i], widget);
        sourceToggles[i]->setObjectName("source" + QString::number(i + 1));
        layout->addWidget(sourceToggles[i]);
    }
    layout->addWidget(label);
    automationLabel = new QLabel(widget);
    automationLabel->setObjectName("automationStatus");
    automationLabel->setWordWrap(true);
    automationLabel->setTextFormat(Qt::PlainText);
    layout->addWidget(automationLabel);
    pipeLabel = new QLabel(widget);
    pipeLabel->setObjectName("originalSdkStatus");
    pipeLabel->setTextFormat(Qt::PlainText);
    pipeLabel->setWordWrap(true);
    layout->addWidget(pipeLabel);
    auto* note = new QLabel("Prepara automáticamente juegos compatibles de tus bibliotecas Wine/Proton.\n"
                           "Si no hay efectos compatibles, se mantiene tu iluminación habitual.\n"
                           "Puedes elegir qué integraciones aceptar. La compatibilidad depende de la versión del SDK que use el juego.", widget);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();
    receiver = new GameReceiver(this);
    receiver->status = [this](QString text) { label->setText(text); };
    receiver->changed = [this] { dirty = true; };
    receiver->released = [this] { relinquish(); };
    bool portValid = false;
    int port = qEnvironmentVariableIntValue("OPENRGB_GAME_SYNC_PORT", &portValid);
    ready = receiver->listen(portValid && port > 0 && port <= 65535 ? quint16(port) : GS_PORT);
    auto settings = api->GetSettings("OpenRGB Game Sync");
    bool enabled = settings.is_object() && settings.contains("enabled") && settings["enabled"].is_boolean()
                   ? settings["enabled"].get<bool>() : true;
    toggle->setChecked(enabled && ready);
    receiver->setEnabled(toggle->isChecked());
    for (unsigned i = 0; i < 3; ++i)
    {
        std::string key = "source" + std::to_string(i + 1);
        bool allowed = !settings.is_object() || !settings.contains(key) || !settings[key].is_boolean() || settings[key].get<bool>();
        sourceToggles[i]->setChecked(allowed);
        receiver->setSourceEnabled(i + 1, allowed);
        connect(sourceToggles[i], &QCheckBox::toggled, this, [this, i](bool value) {
            receiver->setSourceEnabled(i + 1, value);
            saveSettings();
            if (i == 0) manageRest();
            if (i == 1) managePipeServices();
        });
    }
    if (!ready)
    {
        toggle->setEnabled(false);
        label->setText("No se puede abrir el receptor local. Tu efecto habitual sigue activo.");
    }
    connect(toggle, &QCheckBox::toggled, this, [this](bool enabled) {
        receiver->setEnabled(enabled);
        saveSettings();
    });
    renderTimer = new QTimer(this);
    renderTimer->setInterval(33);
    connect(renderTimer, &QTimer::timeout, this, &GameSyncPlugin::render);
    renderTimer->start();
    automationProcess = new QProcess(this);
    connect(automationProcess, &QProcess::errorOccurred, this, [this] {
        automationDeadline->stop();
        if (widget && toggle->isChecked()) automationLabel->setText("No se pudo iniciar la preparación automática.");
    });
    restProcess = new QProcess(this);
    restProcess->setStandardOutputFile(QProcess::nullDevice());
    restProcess->setStandardErrorFile(QProcess::nullDevice());
    connect(restProcess, &QProcess::errorOccurred, this, [this] { if (!widget) return; restUnavailable = true; updateAutomationStatus(); });
    connect(restProcess, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (!widget) return;
        if (toggle->isChecked() && (code || status == QProcess::CrashExit)) { restUnavailable = true; updateAutomationStatus(); }
    });
    automationDeadline = new QTimer(this);
    automationDeadline->setSingleShot(true);
    connect(automationDeadline, &QTimer::timeout, this, [this] {
        if (automationProcess->state() != QProcess::NotRunning) automationProcess->kill();
        automationLabel->setText("Preparación aplazada por tiempo de espera; tu efecto habitual sigue disponible.");
    });
    connect(automationProcess, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        automationDeadline->stop();
        if (!widget || !toggle->isChecked()) return;
        if (code || status == QProcess::CrashExit) automationLabel->setText("La preparación automática no terminó; se reintentará. Tu efecto habitual sigue disponible.");
        else updateAutomationStatus();
    });
    automationTimer = new QTimer(this);
    automationTimer->setInterval(60000);
    connect(automationTimer, &QTimer::timeout, this, &GameSyncPlugin::prepareGames);
    connect(toggle, &QCheckBox::toggled, this, [this](bool active) {
        manageRest();
        managePipeServices();
        if (active) prepareGames();
        else {
            if (automationProcess->state() != QProcess::NotRunning) automationProcess->kill();
            automationLabel->setText("Preparación automática desactivada.");
        }
    });
    automationTimer->start();
    pipeStatusTimer = new QTimer(this);
    pipeStatusTimer->setInterval(1000);
    connect(pipeStatusTimer, &QTimer::timeout, this, &GameSyncPlugin::updatePipeStatus);
    pipeStatusTimer->start();
    manageRest();
    managePipeServices();
    prepareGames();
}

void GameSyncPlugin::prepareGames()
{
    if (!api || !toggle->isChecked() || !automationProcess || automationProcess->state() != QProcess::NotRunning) return;
    auto directory = QString::fromStdString(api->GetConfigurationDirectory());
    if (directory.isEmpty()) return;
    QFile config(directory + "/plugins/settings/GameSyncAutomation.json");
    if (!config.open(QIODevice::ReadOnly)) return;
    auto settings = QJsonDocument::fromJson(config.readAll()).object();
    auto script = settings.value("script").toString();
    automationStatusPath = settings.value("status").toString();
    if (!settings.value("enabled").toBool() || !QFile::exists(script)) return;
    automationProcess->setStandardOutputFile(QProcess::nullDevice());
    automationProcess->setStandardErrorFile(QProcess::nullDevice());
    automationProcess->start("/usr/bin/python3", {script, "--cycle"});
    automationDeadline->start(15000);
}

void GameSyncPlugin::manageRest()
{
    if (!restProcess || !api) return;
    QFile config(QString::fromStdString(api->GetConfigurationDirectory()) + "/plugins/settings/GameSyncAutomation.json");
    QJsonObject settings;
    if (config.open(QIODevice::ReadOnly)) settings = QJsonDocument::fromJson(config.readAll()).object();
    auto script = QFileInfo(settings.value("script").toString()).dir().filePath("chroma_rest.py");
    // REST is a separate opt-in; it never needs launcher/DLL preparation enabled.
    bool wanted = toggle->isChecked() && sourceToggles[0]->isChecked() &&
                  settings.value("rest").toBool() && QFile::exists(script);
    if (!wanted) {
        if (restProcess->state() != QProcess::NotRunning) { restProcess->terminate(); if (!restProcess->waitForFinished(500)) restProcess->kill(); }
        restUnavailable = false;
        return;
    }
    if (restProcess->state() == QProcess::NotRunning)
        restProcess->start("/usr/bin/python3", {script});
}

void GameSyncPlugin::updateAutomationStatus()
{
    if (!widget || !toggle || !toggle->isChecked()) return;
    QFile file(automationStatusPath);
    if (!file.open(QIODevice::ReadOnly)) return;
    auto state = QJsonDocument::fromJson(file.readAll()).object();
    auto errors = state.value("errors").toArray();
    if (restUnavailable) errors.append("Chroma REST no está disponible. Comprueba si el puerto local 54235 está ocupado.");
    auto launch = state.value("launch").toObject();
    for (auto error : launch.value("errors").toArray()) errors.append(error);
    QStringList details;
    for (auto error : errors) details << error.toString();
    automationLabel->setText(errors.isEmpty() ? "Preparación automática activa." :
        QString("Preparación parcial: %1 incidencias. Pasa el ratón para ver los detalles.").arg(errors.size()));
    automationLabel->setToolTip(details.join("\n"));
}

void GameSyncPlugin::managePipeServices(bool stopping)
{
    if (stopping || !toggle || !toggle->isChecked() || !sourceToggles[1]->isChecked()) {
        for (auto* process : pipeProcesses) {
            process->disconnect(this);
            if (process->state() != QProcess::NotRunning) {
                process->terminate();
                if (!process->waitForFinished(1500)) { process->kill(); process->waitForFinished(500); }
            }
            delete process;
        }
        pipeProcesses.clear(); pipeStatusPaths.clear();
        if (pipeLabel) pipeLabel->setText("Servicio iCUE original desactivado.");
        return;
    }
    if (!api || !pipeProcesses.empty()) return;
    QFile config(QString::fromStdString(api->GetConfigurationDirectory()) + "/plugins/settings/GameSyncAutomation.json");
    if (!config.open(QIODevice::ReadOnly)) return;
    auto settings = QJsonDocument::fromJson(config.readAll()).object();
    auto script = QFileInfo(settings.value("script").toString()).dir().filePath("pipe_service/run.py");
    if (!QFile::exists(script)) return;
    for (auto entry : settings.value("pipe_services").toArray()) {
        if (pipeProcesses.size() >= 4) break;
        auto service = entry.toObject();
        auto prefix = service.value("prefix").toString();
        auto statusPath = service.value("status").toString();
        if (prefix.isEmpty() || statusPath.isEmpty() || !QFile::exists(prefix + "/system.reg")) continue;
        auto* process = new QProcess(this);
        QStringList args{script, "--prefix", prefix, "--status", statusPath};
        auto proton = service.value("proton").toString();
        if (!proton.isEmpty()) args << "--umu-proton" << proton;
        else args << "--wine" << service.value("wine").toString("/usr/bin/wine");
        process->setStandardOutputFile(QProcess::nullDevice());
        QDir().mkpath(QFileInfo(statusPath).absolutePath());
        process->setStandardErrorFile(statusPath + ".log", QIODevice::Append);
        pipeProcesses.push_back(process); pipeStatusPaths << statusPath;
        process->start("/usr/bin/python3", args);
    }
    updatePipeStatus();
}

void GameSyncPlugin::updatePipeStatus()
{
    if (!widget || !toggle || !toggle->isChecked() || !sourceToggles[1]->isChecked()) return;
    if (pipeProcesses.empty()) return;
    int connected = 0, calls = 0; QStringList errors; bool failed = false, ready = false;
    for (size_t i=0; i<pipeProcesses.size(); ++i) {
        failed |= pipeProcesses[i]->state() == QProcess::NotRunning;
        QFile file(pipeStatusPaths[int(i)]);
        if (!file.open(QIODevice::ReadOnly)) continue;
        auto state = QJsonDocument::fromJson(file.readAll()).object();
        ready |= state.value("running").toBool();
        connected += state.value("sessions").toInt();
        auto methods = state.value("calls").toObject();
        calls += methods.value("CorsairSetLedsColors").toInt() + methods.value("CorsairSetLedsColorsFlushBuffer").toInt();
        for (auto error : state.value("errors").toArray()) errors << error.toString();
    }
    pipeLabel->setText(failed ? "El servicio iCUE original se ha detenido; consulta su registro." :
        !ready ? "Iniciando servicio para la DLL original de iCUE…" :
        !errors.isEmpty() ? "Servicio iCUE original activo con funciones pendientes (detalles al pasar el ratón)." :
        QString("iCUE original · %1 conexiones · %2 actualizaciones de color").arg(connected).arg(calls));
    pipeLabel->setToolTip(errors.join("\n") + "\n" + pipeStatusPaths.join("\n"));
}

void GameSyncPlugin::saveSettings()
{
    nlohmann::json settings = {{"enabled", toggle->isChecked()}};
    for (unsigned i = 0; i < 3; ++i) settings["source" + std::to_string(i + 1)] = sourceToggles[i]->isChecked();
    api->SetSettings("OpenRGB Game Sync", settings);
    api->SaveSettings();
}

void GameSyncPlugin::relinquish()
{
    std::lock_guard<std::mutex> devices(devicesMutex);
    PriorityLock lock(lease);
    if (api && lock.acquired && !detecting)
    {
        auto live = api->GetRGBControllers();
        for (const auto& [controller, saved] : snapshots)
        {
            if (std::find(live.begin(), live.end(), controller) == live.end()) continue;
            for (unsigned i = 0; i < saved.colors.size() && i < controller->GetLEDCount(); ++i)
                controller->SetColor(i, saved.colors[i]);
            if (saved.mode >= 0) { controller->SetActiveMode(saved.mode); controller->UpdateMode(); }
            controller->UpdateLEDs();
        }
    }
    snapshots.clear();
    lease.release();
    dirty = false;
}

static RGBColor average(const GameFrame& frame)
{
    unsigned r = 0, g = 0, b = 0, n = 0;
    for (auto c : frame.colors)
    {
        if (!c) continue;
        r += RGBGetRValue(c); g += RGBGetGValue(c); b += RGBGetBValue(c); ++n;
    }
    return n ? ToRGBColor(r / n, g / n, b / n) : 0;
}

void GameSyncPlugin::render()
{
    if (!receiver || !receiver->active() || !toggle->isChecked()) return;
    std::lock_guard<std::mutex> deviceLock(devicesMutex);
    if (detecting) return;
    PriorityLock priority(lease);
    if (!priority.acquired) return; // Never compete with the ordinary renderer.
    lease.renew();
    if (!dirty) return;
    const auto& frames = receiver->frames;
    const auto& ambient = frames.contains(6) ? frames[6] : (frames.contains(1) ? frames[1] : frames.first());
    for (auto* controller : api->GetRGBControllers())
    {
        if (controller->GetHidden() || !controller->GetLEDCount()) continue;
        int direct = -1;
        for (unsigned m = 0; m < controller->GetModeCount(); ++m)
            if (controller->GetModeName(m) == "Direct") { direct = int(m); break; }
        if (direct < 0) continue;
        if (!snapshots.count(controller))
        {
            Snapshot saved{controller->GetActiveMode(), {}};
            for (unsigned i = 0; i < controller->GetLEDCount(); ++i) saved.colors.push_back(controller->GetColor(i));
            snapshots.emplace(controller, std::move(saved));
        }
        if (controller->GetActiveMode() != direct) { controller->SetActiveMode(direct); controller->UpdateMode(); }
        const bool keyboard = controller->GetDeviceType() == DEVICE_TYPE_KEYBOARD;
        if (keyboard && frames.contains(1))
        {
            const auto& f = frames[1];
            controller->SetAllColors(average(f));
            for (unsigned z = 0; z < controller->GetZoneCount(); ++z)
            {
                unsigned h = controller->GetZoneMatrixMapHeight(z), w = controller->GetZoneMatrixMapWidth(z);
                const auto* map = controller->GetZoneMatrixMapData(z);
                if (!h || !w || !map) continue;
                for (unsigned y = 0; y < h; ++y) for (unsigned x = 0; x < w; ++x)
                {
                    unsigned led = map[y * w + x];
                    if (led >= controller->GetLEDCount()) continue;
                    unsigned row = h > 1 ? y * (f.rows - 1) / (h - 1) : 0;
                    unsigned col = w > 1 ? x * (f.cols - 1) / (w - 1) : 0;
                    controller->SetColor(led, f.colors[int(row * f.cols + col)]);
                }
            }
        }
        else
        {
            unsigned role = 0;
            switch (controller->GetDeviceType())
            {
                case DEVICE_TYPE_MOUSE: role = 2; break;
                case DEVICE_TYPE_HEADSET: role = 3; break;
                case DEVICE_TYPE_MOUSEMAT: role = 4; break;
                case DEVICE_TYPE_KEYPAD: role = 5; break;
                default: break;
            }
            controller->SetAllColors(average(frames.contains(role) ? frames[role] : ambient));
        }
        controller->UpdateLEDs();
    }
    dirty = false;
}

void GameSyncPlugin::ResourceManagerUpdated(unsigned int reason)
{
    std::lock_guard<std::mutex> lock(devicesMutex);
    if (reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_STARTED)
    {
        detecting = true;
        snapshots.clear(); // Device handles may be invalidated during a scan.
    }
    if (reason == RESOURCEMANAGER_UPDATE_REASON_DETECTION_COMPLETE || reason == RESOURCEMANAGER_UPDATE_REASON_DEVICE_LIST_UPDATED)
    {
        detecting = false;
        QMetaObject::invokeMethod(this, [this] { dirty = true; }, Qt::QueuedConnection);
    }
}

void GameSyncPlugin::OnProfileAboutToLoad()
{
    if (receiver) receiver->release();
}

void GameSyncPlugin::Unload()
{
    if (pipeStatusTimer) pipeStatusTimer->stop();
    managePipeServices(true);
    if (automationTimer) automationTimer->stop();
    if (automationDeadline) automationDeadline->stop();
    if (restProcess && restProcess->state() != QProcess::NotRunning) {
        restProcess->terminate(); if (!restProcess->waitForFinished(500)) { restProcess->kill(); restProcess->waitForFinished(500); }
    }
    if (automationProcess && automationProcess->state() != QProcess::NotRunning) {
        automationProcess->terminate();
        if (!automationProcess->waitForFinished(1000)) { automationProcess->kill(); automationProcess->waitForFinished(1000); }
    }
    if (renderTimer) renderTimer->stop();
    if (receiver) { receiver->release(); delete receiver; receiver = nullptr; }
    relinquish();
    delete widget;
    widget = nullptr;
    toggle = nullptr;
    automationLabel = nullptr;
    pipeLabel = nullptr;
    api = nullptr;
}
